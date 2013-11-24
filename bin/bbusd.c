/*
 * Copyright (C) 2013 Bartosz Golaszewski <bartekgola@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <busybus.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include "bbusd/log.h"
#include "bbusd/common.h"

struct option_flags
{
	int log_to_console;
	int log_to_syslog;
};

struct clientlist_elem
{
	struct clientlist_elem* next;
	struct clientlist_elem* prev;
	bbus_client* cli;
};

struct clientlist
{
	struct clientlist_elem* head;
	struct clientlist_elem* tail;
};

#define METHOD_LOCAL	0x01
#define METHOD_REMOTE	0x02

struct method
{
	int type;
	char data[0];
};

struct local_method
{
	int type;
	bbus_method_func func;
};

#define DEF_LOCAL_METHOD(FUNC)						\
	static struct local_method __m_##FUNC##__ = {			\
		.type = METHOD_LOCAL,					\
		.func = FUNC,						\
	}

#define REG_LOCAL_METHOD(PATH, FUNC)					\
	do {								\
		if (insert_method(PATH,					\
				(struct method*)&__m_##FUNC##__) < 0) {	\
			bbusd_die(					\
				"Error inserting method: '%s'\n",	\
				PATH);					\
		}							\
	} while (0)

struct remote_method
{
	int type;
	struct clientlist_elem* srvc;
};

struct service_tree
{
	/* Values are pointers to struct service_map. */
	bbus_hashmap* subsrvc;
	/* Values are pointers to struct method. */
	bbus_hashmap* methods;
};

static bbus_server* server;
static struct clientlist clients = { NULL, NULL };
static struct clientlist monitors = { NULL, NULL };
static volatile int run;
static bbus_hashmap* caller_map;
static struct service_tree* srvc_tree;
static unsigned char _msgbuf[BBUS_MAXMSGSIZE];
static struct bbus_msg* msgbuf = (struct bbus_msg*)_msgbuf;

static struct bbus_option cmdopts[] = {
	{
		.shortopt = 0,
		.longopt = "sockpath",
		.hasarg = BBUS_OPT_ARGREQ,
		.action = BBUS_OPTACT_CALLFUNC,
		.actdata = &bbus_prot_setsockpath,
		.descr = "path to the busybus socket",
	}
};

static struct bbus_opt_list optlist = {
	.opts = cmdopts,
	.numopts = BBUS_ARRAY_SIZE(cmdopts),
	.progname = "Busybus",
	.version = "ALPHA",
	.progdescr = "Tiny message bus daemon."
};

static bbus_object* lm_echo(bbus_object* arg)
{
	char* msg;
	int ret;

	ret = bbus_obj_parse(arg, "s", &msg);
	if (ret < 0)
		return NULL;
	else
		return bbus_obj_build("s", msg);
}
DEF_LOCAL_METHOD(lm_echo);

static int do_run(void)
{
	return BBUS_ATOMIC_GET(run);
}

static void do_stop(void)
{
	BBUS_ATOMIC_SET(run, 0);
}

static void sighandler(int signum)
{
	switch (signum) {
	case SIGINT:
	case SIGTERM:
		do_stop();
		break;
	}
}

static int list_add(bbus_client* cli, struct clientlist* list)
{
	struct clientlist_elem* el;

	el = bbus_malloc(sizeof(struct clientlist_elem));
	if (el == NULL)
		return -1;

	el->cli = cli;
	bbus_list_push(list, el);

	return 0;
}

static void list_rm(struct clientlist_elem** elem, struct clientlist* list)
{
	bbus_list_rm(list, *elem);
	bbus_free(*elem);
	*elem = NULL;
}

static int client_list_add(bbus_client* cli)
{
	return list_add(cli, &clients);
}

static int monitor_list_add(bbus_client* cli)
{
	return list_add(cli, &monitors);
}

static int do_insert_method(const char* path, struct method* mthd,
					struct service_tree* node)
{
	char* found;
	struct service_tree* next;
	int ret;
	void* mval;

	found = index(path, '.');
	if (found == NULL) {
		/* Path is the method name. */
		mval = bbus_hmap_findstr(node->methods, path);
		if (mval != NULL) {
			bbusd_logmsg(BBUS_LOG_ERR,
				"Method already exists for this value: %s\n",
				path);
			return -1;
		}
		ret = bbus_hmap_setstr(node->methods, path, mthd);
		if (ret < 0) {
			bbusd_logmsg(BBUS_LOG_ERR,
				"Error registering new method: %s\n",
				bbus_strerror(bbus_lasterror()));
			return -1;
		}

		return 0;
	} else {
		/* Path is the subservice name. */
		*found = '\0';
		next = bbus_hmap_findstr(node->subsrvc, path);
		if (next == NULL) {
			/* Insert new service. */
			next = bbus_malloc(sizeof(struct service_tree));
			if (next == NULL)
				goto err_mknext;

			next->subsrvc = bbus_hmap_create(BBUS_HMAP_KEYSTR);
			if (next->subsrvc == NULL)
				goto err_mksubsrvc;

			next->methods = bbus_hmap_create(BBUS_HMAP_KEYSTR);
			if (next->subsrvc == NULL)
				goto err_mkmethods;

			ret = bbus_hmap_setstr(node->subsrvc, path, next);
			if (ret < 0)
				goto err_setsrvc;
		}

		return do_insert_method(found+1, mthd, next);
	}

err_setsrvc:
	bbus_hmap_free(next->methods);

err_mkmethods:
	bbus_hmap_free(next->subsrvc);

err_mksubsrvc:
	bbus_free(next);

err_mknext:
	return -1;
}

static int insert_method(const char* path, struct method* mthd)
{
	char* mname;
	int ret;

	mname = bbus_str_cpy(path);
	if (mname == NULL)
		return -1;

	ret = do_insert_method(mname, mthd, srvc_tree);
	bbus_str_free(mname);

	return ret;
}

static struct method* do_locate_method(char* path, struct service_tree* node)
{
	char* found;
	struct service_tree* next;

	found = index(path, '.');
	if (found == NULL) {
		/* This is a method. */
		return bbus_hmap_findstr(node->methods, path);
	} else {
		*found = '\0';
		/* This is a sub-service. */
		next = bbus_hmap_findstr(node->subsrvc, path);
		if (next == NULL) {
			return NULL;
		}

		return do_locate_method(found+1, next);
	}
}

static struct method* locate_method(const char* path)
{
	char* mname;
	struct method* ret;

	mname = bbus_str_cpy(path);
	if (mname == NULL)
		return NULL;

	ret = do_locate_method(mname, srvc_tree);
	bbus_str_free(mname);

	return ret;
}

static void send_to_monitors(struct bbus_msg* msg BBUS_UNUSED)
{
	return;
}

static char* mname_from_srvcname(const char* srvc)
{
	char* found;

	found = rindex(srvc, '.');
	if (found != NULL)
		++found;
	return found;
}

static int handle_clientcall(bbus_client* cli, struct bbus_msg* msg)
{
	struct method* mthd;
	const char* mname;
	int ret;
	bbus_object* argobj = NULL;
	bbus_object* retobj = NULL;
	struct bbus_msg_hdr hdr;
	char* meta;

	mname = bbus_prot_extractmeta(msg);
	if (mname == NULL)
		return -1;

	memset(&hdr, 0, sizeof(struct bbus_msg_hdr));
	mthd = locate_method(mname);
	if (mthd == NULL) {
		bbusd_logmsg(BBUS_LOG_ERR, "No such method: %s\n", mname);
		bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY,
					BBUS_PROT_ENOMETHOD);
		ret = -1;
		goto respond;
	}

	argobj = bbus_prot_extractobj(msg);
	if (argobj == NULL)
		return -1;

	if (mthd->type == METHOD_LOCAL) {
		retobj = ((struct local_method*)mthd)->func(argobj);
		if (retobj == NULL) {
			bbusd_logmsg(BBUS_LOG_ERR, "Error calling method.\n");
			bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY,
					BBUS_PROT_EMETHODERR);
		} else {
			bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY,
					BBUS_PROT_EGOOD);
			bbus_hdr_setpsize(&hdr, bbus_obj_rawsize(retobj));
		}

		goto respond;
	} else
	if (mthd->type == METHOD_REMOTE) {
		meta = mname_from_srvcname(mname);
		if (meta == NULL) {
			bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY,
					BBUS_PROT_EMETHODERR);
			goto respond;
		}
		bbus_hdr_build(&hdr, BBUS_MSGTYPE_SRVCALL, BBUS_PROT_EGOOD);
		BBUS_HDR_SETFLAG(&hdr, BBUS_PROT_HASMETA);
		BBUS_HDR_SETFLAG(&hdr, BBUS_PROT_HASOBJECT);
		bbus_hdr_setpsize(&hdr, (uint16_t)(strlen(meta) + 1
						+ bbus_obj_rawsize(argobj)));
		bbus_hdr_settoken(&hdr, bbus_client_gettoken(cli));

		ret = bbus_client_sendmsg(
				((struct remote_method*)mthd)->srvc->cli,
				&hdr, meta, argobj);
		if (ret < 0) {
			bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY,
					BBUS_PROT_EMETHODERR);
			goto respond;
		}
	} else {
		bbusd_die("Internal logic error, invalid method type\n");
	}

	goto dontrespond;

respond:
	ret = bbus_client_sendmsg(cli, &hdr, NULL, retobj);
	if (ret < 0) {
		bbusd_logmsg(BBUS_LOG_ERR,
				"Error sending reply to client: %s\n",
				bbus_strerror(bbus_lasterror()));
		ret = -1;
	}

	bbus_obj_free(retobj);

dontrespond:
	bbus_obj_free(argobj);
	return ret;
}

static int register_service(struct clientlist_elem* cli, struct bbus_msg* msg)
{
	const char* extrmeta;
	char* meta;
	int ret;
	char* comma;
	char* path;
	struct remote_method* mthd;
	struct bbus_msg_hdr hdr;

	extrmeta = bbus_prot_extractmeta(msg);
	if (extrmeta == NULL) {
		ret = -1;
		goto respond;
	}

	meta = bbus_str_cpy(extrmeta);
	if (meta == NULL) {
		ret = -1;
		goto metafree;
	}

	comma = index(meta, ',');
	if (comma == NULL) {
		ret = -1;
		goto metafree;
	}
	*comma = '\0';

	path = bbus_str_build("bbus.%s", meta);
	if (path == NULL) {
		ret = -1;
		goto metafree;
	}

	mthd = bbus_malloc0(sizeof(struct remote_method));
	if (mthd == NULL) {
		ret = -1;
		goto pathfree;
	}

	mthd->type = METHOD_REMOTE;
	mthd->srvc = cli;

	ret = insert_method(path, (struct method*)mthd);
	if (ret < 0) {
		ret = -1;
		goto mthdfree;
	} else {
		bbusd_logmsg(BBUS_LOG_INFO,
			"Method '%s' successfully registered.\n", path);
		ret = 0;
		goto pathfree;
	}

mthdfree:
	bbus_free(mthd);

pathfree:
	bbus_str_free(path);

metafree:
	bbus_str_free(meta);

respond:
	bbus_hdr_build(&hdr, BBUS_MSGTYPE_SRVACK, ret == 0
				? BBUS_PROT_EGOOD : BBUS_PROT_EMREGERR);
	ret = bbus_client_sendmsg(cli->cli, &hdr, NULL, NULL);
	if (ret < 0) {
		bbusd_logmsg(BBUS_LOG_ERR,
				"Error sending reply to client: %s\n",
				bbus_strerror(bbus_lasterror()));
		ret = -1;
	}

	return ret;
}

static int unregister_service(bbus_client* cli BBUS_UNUSED,
		const struct bbus_msg* msg BBUS_UNUSED)
{
	return 0;
}

static int handle_control_message(bbus_client* cli BBUS_UNUSED,
		const struct bbus_msg* msg BBUS_UNUSED)
{
	return 0;
}

static int pass_srvc_reply(bbus_client* srvc BBUS_UNUSED, struct bbus_msg* msg)
{
	struct bbus_msg_hdr hdr;
	struct clientlist_elem* cli;
	bbus_object* obj;
	int ret;

	cli = (struct clientlist_elem*)bbus_hmap_finduint(caller_map,
						bbus_hdr_gettoken(&msg->hdr));
	if (cli == NULL) {
		bbusd_logmsg(BBUS_LOG_ERR, "Caller not found for reply.\n");
		return -1;
	}

	obj = bbus_prot_extractobj(msg);
	if (obj == NULL) {
		bbusd_logmsg(BBUS_LOG_ERR,
			"Error extracting the object from message: %s\n",
			bbus_strerror(bbus_lasterror()));
		bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY,
					BBUS_PROT_EMETHODERR);
		goto respond;
	}

	bbus_hdr_build(&hdr, BBUS_MSGTYPE_CLIREPLY, BBUS_PROT_EGOOD);
	BBUS_HDR_SETFLAG(&hdr, BBUS_PROT_HASOBJECT);
	bbus_hdr_setpsize(&hdr, bbus_obj_rawsize(obj));

respond:
	ret = bbus_client_sendmsg(cli->cli, &hdr, NULL, obj);
	if (ret < 0) {
		bbusd_logmsg(BBUS_LOG_ERR,
			"Error sending server reply to client: %s\n",
			bbus_strerror(bbus_lasterror()));
		ret = -1;
	}

	bbus_obj_free(obj);
	return ret;
}

static uint32_t make_token(void)
{
	static uint32_t curtok = 0;

	if (curtok == UINT_MAX)
		curtok = 0;

	return ++curtok;
}

static void accept_client(void)
{
	bbus_client* cli;
	int r;
	uint32_t token;

	/* TODO Client credentials verification. */
	cli = bbus_srv_accept(server);
	if (cli == NULL) {
		bbusd_logmsg(BBUS_LOG_ERR,
			"Error accepting incoming client "
			"connection: %s\n",
			bbus_strerror(bbus_lasterror()));
		return;
	}
	bbusd_logmsg(BBUS_LOG_INFO, "Client connected.\n");

	r = client_list_add(cli);
	if (r < 0) {
		bbusd_logmsg(BBUS_LOG_ERR,
			"Error adding new client to the list: %s\n",
			bbus_strerror(bbus_lasterror()));
		return;
	}

	switch (bbus_client_gettype(cli)) {
	case BBUS_CLIENT_CALLER:
		token = make_token();
		bbus_client_settoken(cli, token);
		/* This client is the list's tail at this point. */
		r = bbus_hmap_setuint(caller_map, (unsigned)token,
							clients.tail);
		if (r < 0) {
			bbusd_logmsg(BBUS_LOG_ERR,
				"Error adding new client to "
				"the caller map: %s\n",
				bbus_strerror(bbus_lasterror()));
		}
		break;
	case BBUS_CLIENT_MON:
		r = monitor_list_add(cli);
		if (r < 0) {
			bbusd_logmsg(BBUS_LOG_ERR,
				"Error adding new monitor to "
				"the list: %s\n",
				bbus_strerror(bbus_lasterror()));
			return;
		}
		break;
	case BBUS_CLIENT_SERVICE:
	case BBUS_CLIENT_CTL:
		/*
		 * Don't do anything else other than adding these
		 * clients to the main client list.
		 */
		break;
	default:
		break;
	}
}

static void handle_client(struct clientlist_elem** cli_elem)
{
	bbus_client* cli;
	int r;

	cli = (*cli_elem)->cli;
	memset(msgbuf, 0, BBUS_MAXMSGSIZE);
	r = bbus_client_rcvmsg(cli, msgbuf, BBUS_MAXMSGSIZE);
	if (r < 0) {
		bbusd_logmsg(BBUS_LOG_ERR,
			"Error receiving message from client: %s\n",
			bbus_strerror(bbus_lasterror()));
		goto cli_close;
	}

	send_to_monitors(msgbuf);

	/* TODO Common function for error reporting. */
	switch (bbus_client_gettype(cli)) {
	case BBUS_CLIENT_CALLER:
		switch (msgbuf->hdr.msgtype) {
		case BBUS_MSGTYPE_CLICALL:
			r = handle_clientcall(cli, msgbuf);
			if (r < 0) {
				bbusd_logmsg(BBUS_LOG_ERR,
					"Error on client call\n");
				goto cli_close;
			}
			break;
		case BBUS_MSGTYPE_CLOSE:
			goto cli_close;
			break;
		default:
			bbusd_logmsg(BBUS_LOG_ERR,
					"Unexpected message received.\n");
			goto cli_close;
			break;
		}
		break;
	case BBUS_CLIENT_SERVICE:
		switch (msgbuf->hdr.msgtype) {
		case BBUS_MSGTYPE_SRVREG:
			r = register_service(*cli_elem, msgbuf);
			if (r < 0) {
				bbusd_logmsg(BBUS_LOG_ERR,
					"Error registering a service\n");
				return;
			}
			break;
		case BBUS_MSGTYPE_SRVUNREG:
			r = unregister_service(cli, msgbuf);
			if (r < 0) {
				bbusd_logmsg(BBUS_LOG_ERR,
					"Error unregistering a service: %s\n",
					bbus_strerror(bbus_lasterror()));
				return;
			}
			break;
		case BBUS_MSGTYPE_SRVREPLY:
			r = pass_srvc_reply(cli, msgbuf);
			if (r < 0) {
				bbusd_logmsg(BBUS_LOG_ERR,
					"Error passing a service reply: %s\n",
					bbus_strerror(bbus_lasterror()));
				return;
			}
			break;
		case BBUS_MSGTYPE_CLOSE:
			goto cli_close;
			break;
		default:
			bbusd_logmsg(BBUS_LOG_ERR,
					"Unexpected message received.\n");
			goto cli_close;
			return;
		}
		break;
	case BBUS_CLIENT_CTL:
		switch (msgbuf->hdr.msgtype) {
		case BBUS_MSGTYPE_CTRL:
			handle_control_message(cli, msgbuf);
			break;
		case BBUS_MSGTYPE_CLOSE:
			goto cli_close;
			break;
		default:
			bbusd_logmsg(BBUS_LOG_ERR,
					"Unexpected message received.\n");
			return;
		}
		break;
	case BBUS_CLIENT_MON:
		switch (msgbuf->hdr.msgtype) {
		case BBUS_MSGTYPE_CLOSE:
			{
				struct clientlist_elem* mon;

				for (mon = monitors.head; mon != NULL;
							mon = mon->next) {
					if (mon->cli == cli) {
						list_rm(&mon, &monitors);
						goto cli_close;
					}
				}
				bbusd_logmsg(BBUS_LOG_WARN,
					"Monitor not found in the list, "
					"this should not happen.\n");
				goto cli_close;
			}
			break;
		default:
			bbusd_logmsg(BBUS_LOG_WARN,
				"Message received from a monitor which should "
				"not be sending any messages - discarding.\n");
			goto cli_close;
			break;
		}
		break;
	default:
		bbusd_logmsg(BBUS_LOG_ERR,
			"Unhandled client type in the received message.\n");
		return;
	}

	return;

cli_close:
	bbus_client_close(cli);
	bbus_client_free(cli);
	list_rm(cli_elem, &clients);
	bbusd_logmsg(BBUS_LOG_INFO, "Client disconnected.\n");
}

int main(int argc, char** argv)
{
	int retval;
	struct clientlist_elem* tmpcli;
	struct bbus_timeval tv;
	static bbus_pollset* pollset;

	retval = bbus_parse_args(argc, argv, &optlist, NULL);
	if (retval == BBUS_ARGS_HELP)
		return 0;
	else if (retval == BBUS_ARGS_ERR)
		return -1;

	/*
	 * Caller map:
	 * 	keys -> tokens,
	 * 	values -> pointers to caller objects.
	 */
	caller_map = bbus_hmap_create(BBUS_HMAP_KEYUINT);
	if (caller_map == NULL) {
		bbusd_die("Error creating the caller hashmap: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	/* Service map. */
	srvc_tree = bbus_malloc(sizeof(struct service_tree));
	if (srvc_tree == NULL)
		goto err_map;

	srvc_tree->subsrvc = bbus_hmap_create(BBUS_HMAP_KEYSTR);
	if (srvc_tree->subsrvc == NULL) {
		bbus_free(srvc_tree);
		goto err_map;
	}

	srvc_tree->methods = bbus_hmap_create(BBUS_HMAP_KEYSTR);
	if (srvc_tree->methods == NULL) {
		bbus_hmap_free(srvc_tree->subsrvc);
		bbus_free(srvc_tree);
		goto err_map;
	}

	/* Creating the server object. */
	server = bbus_srv_create();
	if (server == NULL) {
		bbusd_die("Error creating the server object: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	retval = bbus_srv_listen(server);
	if (retval < 0) {
		bbusd_die("Error opening server for connections: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	pollset = bbus_pollset_make();
	if (pollset == NULL) {
		bbusd_die("Error creating the poll_set: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	REG_LOCAL_METHOD("bbus.bbusd.echo", lm_echo);

	bbusd_logmsg(BBUS_LOG_INFO, "Busybus daemon starting!\n");
	run = 1;
	(void)signal(SIGTERM, sighandler);
	(void)signal(SIGINT, sighandler);
	(void)signal(SIGPIPE, SIG_IGN);

	/*
	 * MAIN LOOP
	 */
	while (do_run()) {
		memset(&tv, 0, sizeof(struct bbus_timeval));
		bbus_pollset_clear(pollset);
		bbus_pollset_addsrv(pollset, server);
		for (tmpcli = clients.head; tmpcli != NULL;
				tmpcli = tmpcli->next) {
			bbus_pollset_addcli(pollset, tmpcli->cli);
		}
		tv.sec = 0;
		tv.usec = 500000;
		retval = bbus_poll(pollset, &tv);
		if (retval < 0) {
			if (bbus_lasterror() == BBUS_EPOLLINTR) {
				continue;
			} else {
				bbusd_die("Error polling connections: %s",
					bbus_strerror(bbus_lasterror()));
			}
		} else
		if (retval == 0) {
			/* Timeout. */
			continue;
		} else {
			/* Incoming data. */
			if (bbus_pollset_srvisset(pollset, server)) {
				while (bbus_srv_clientpending(server)) {
					accept_client();
				}
				--retval;
			}

			tmpcli = clients.head;
			while (retval > 0) {
				if (bbus_pollset_cliisset(pollset,
							tmpcli->cli)) {
					handle_client(&tmpcli);
					--retval;
				}
				if (tmpcli != NULL)
					tmpcli = tmpcli->next;
			}
		}
	}
	/*
	 * END OF THE MAIN LOOP
	 */

	/* Cleanup. */
	bbus_srv_close(server);

	for (tmpcli = clients.head; tmpcli != NULL; tmpcli = tmpcli->next) {
		bbus_client_close(tmpcli->cli);
		bbus_client_free(tmpcli->cli);
		bbus_free(tmpcli);
	}

	for (tmpcli = monitors.head; tmpcli != NULL; tmpcli = tmpcli->next) {
		bbus_free(tmpcli);
	}

	bbusd_logmsg(BBUS_LOG_INFO, "Busybus daemon exiting!\n");
	return 0;

err_map:
	bbusd_die("Error creating the service map: %s\n",
		bbus_strerror(bbus_lasterror()));
}

