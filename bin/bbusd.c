/*
 * Copyright (C) 2013 Bartosz Golaszewski
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
#include "common.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <getopt.h>
#include <signal.h>
#include <syslog.h>
#include <limits.h>
#include <search.h>
#include <string.h>

#define SYSLOG_IDENT "bbusd"

struct option_flags
{
	int print_help;
	int print_version;
	int log_to_console;
	int log_to_syslog;
};

enum loglevel
{
	BBUS_LOG_EMERG = LOG_EMERG,
	BBUS_LOG_ALERT = LOG_ALERT,
	BBUS_LOG_CRIT = LOG_CRIT,
	BBUS_LOG_ERR = LOG_ERR,
	BBUS_LOG_WARN = LOG_WARNING,
	BBUS_LOG_NOTICE = LOG_NOTICE,
	BBUS_LOG_INFO = LOG_INFO,
	BBUS_LOG_DEBUG = LOG_DEBUG
};

struct clientlist_elem
{
	struct clientlist_elem* next;
	struct clientlist_elem* prev;
	bbus_client* cli;
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

struct remote_method
{
	int type;
	struct clientlist_elem* srvc;
};

struct service_map
{
	bbus_hashmap* subsrvc;
	/* Map's values are pointers to struct method. */
	bbus_hashmap* methods;
};

static char* sockpath = BBUS_DEF_DIRPATH BBUS_DEF_SOCKNAME;
static bbus_server* server;
static struct clientlist_elem* clients_head = NULL;
static struct clientlist_elem* clients_tail = NULL;
static struct clientlist_elem* monitors_head = NULL;
static struct clientlist_elem* monitors_tail = NULL;
static bbus_pollset* pollset;
static int run;
/* TODO in the future syslog will be the default. */
static struct option_flags options = { 0, 0, 1, 0 };
static bbus_hashmap* caller_map;
static struct service_map* srvc_map;
static char msgbuf[BBUS_MAXMSGSIZE];

static void print_help_and_exit(void)
{
	fprintf(stdout, "Help stub\n");
	exit(EXIT_SUCCESS);
}

static void print_version_and_exit(void)
{
	fprintf(stdout, "Version stub\n");
	exit(EXIT_SUCCESS);
}

static int loglvl_to_sysloglvl(enum loglevel lvl)
{
	/* TODO Make sure it works properly. */
	return (int)lvl;
}

static void BBUS_PRINTF_FUNC(2, 3) logmsg(enum loglevel lvl,
						const char* fmt, ...)
{
	va_list va;

	if (options.log_to_console) {
		va_start(va, fmt);
		switch (lvl) {
		case BBUS_LOG_EMERG:
		case BBUS_LOG_ALERT:
		case BBUS_LOG_CRIT:
		case BBUS_LOG_ERR:
		case BBUS_LOG_WARN:
			vfprintf(stderr, fmt, va);
			break;
		case BBUS_LOG_NOTICE:
		case BBUS_LOG_INFO:
		case BBUS_LOG_DEBUG:
			vfprintf(stdout, fmt, va);
			break;
		default:
			die("Invalid log level\n");
			break;
		}
		va_end(va);
	}

	if (options.log_to_syslog) {
		va_start(va, fmt);
		openlog(SYSLOG_IDENT, LOG_PID, LOG_DAEMON);
		vsyslog(loglvl_to_sysloglvl(lvl), fmt, va);
		closelog();
		va_end(va);
	}
}

static void parse_args(int argc, char** argv)
{
	static const struct option longopts[] = {
		{ "help", no_argument, &options.print_help, 1 },
		{ "version", no_argument, &options.print_version, 1 },
		{ "sockpath", required_argument, 0, 's' },
		{ 0, 0, 0, 0 }
	};

	static const char* const shortopts = "hvs:";

	int opt, index;

	while ((opt = getopt_long(argc, argv, shortopts,
				longopts, &index)) != -1) {
		switch (opt) {
		case 's':
			sockpath = optarg;
			break;
		case '?':
			break;
		case 0:
			break;
		default:
			die("Invalid argument\n");
			break;
		}
	}
}

static int do_run(void)
{
	return __sync_fetch_and_or(&run, 0);
}

static void do_stop(void)
{
	__sync_lock_test_and_set(&run, 0);
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

static int client_list_add(bbus_client* cli, struct clientlist_elem** head,
					struct clientlist_elem** tail)
{
	struct clientlist_elem* el;

	el = bbus_malloc(sizeof(struct clientlist_elem));
	if (el == NULL)
		return -1;

	el->cli = cli;
	if (*tail == NULL) {
		*head = *tail = el;
		el->prev = NULL;
		el->next = NULL;
	} else {
		insque(el, *tail);
	}

	return 0;
}

static struct method* locate_method(const char* method BBUS_UNUSED)
{
	return NULL;
}

static int call_local_method(bbus_client* cli BBUS_UNUSED,
				const struct bbus_msg* msg BBUS_UNUSED,
				const struct local_method* mthd BBUS_UNUSED)
{
	return 0;
}

static int call_remote_method(bbus_client* cli BBUS_UNUSED,
				const struct bbus_msg* msg BBUS_UNUSED,
				const struct remote_method* mthd BBUS_UNUSED)
{
	return 0;
}

static int handle_clientcall(bbus_client* cli, struct bbus_msg* msg)
{
	struct method* mthd;
	char* mname;
	int ret;

	mname = msg->payload;
	mthd = locate_method(mname);
	if (mthd == NULL) {
		logmsg(BBUS_LOG_ERR, "No such method: %s\n", mname);
		return -1;
	} else {
		if (mthd->type == METHOD_LOCAL) {
			ret = call_local_method(cli, msg,
					(struct local_method*)mthd);
		} else
		if (mthd->type == METHOD_REMOTE) {
			ret = call_remote_method(cli, msg,
					(struct remote_method*)mthd);
		} else {
			die("Internal logic error, invalid method type\n");
		}
	}

	return ret;
}

static int register_service(bbus_client* cli BBUS_UNUSED,
		const struct bbus_msg* msg BBUS_UNUSED)
{
	return 0;
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
	struct bbus_msg_hdr* hdr = &msg->hdr;
	struct clientlist_elem* cli;

	cli = (struct clientlist_elem*)bbus_hmap_find(caller_map, &hdr->token,
							sizeof(hdr->token));
	if (cli == NULL) {
		logmsg(BBUS_LOG_ERR, "Caller not found for reply.\n");
		return -1;
	}

	return -1;
}

static uint32_t make_token(void)
{
	static uint32_t curtok = 0;

	if (curtok == UINT_MAX)
		curtok = 0;

	return ++curtok;
}

static void accept_clients(void)
{
	bbus_client* cli;
	int r;
	uint32_t token;

	while (bbus_srv_clientpending(server)) {
		cli = bbus_srv_accept(server);
		if (cli == NULL) {
			logmsg(BBUS_LOG_ERR,
				"Error accepting incoming client "
				"connection: %s\n",
				bbus_strerror(bbus_lasterror()));
			continue;
		}
		logmsg(BBUS_LOG_INFO, "Client connected.\n");

		r = client_list_add(cli, &clients_head, &clients_tail);
		if (r < 0) {
			logmsg(BBUS_LOG_ERR,
				"Error adding new client to the list: %s\n",
				bbus_strerror(bbus_lasterror()));
			continue;
		}

		switch (bbus_client_gettype(cli)) {
		case BBUS_CLIENT_CALLER:
			token = make_token();
			bbus_client_settoken(cli, token);
			/* This client is the list's tail at this point. */
			r = bbus_hmap_set(caller_map, &token,
						sizeof(token), clients_tail);
			if (r < 0) {
				logmsg(BBUS_LOG_ERR,
					"Error adding new client to "
					"the caller map: %s\n",
					bbus_strerror(bbus_lasterror()));
			}
			break;
		case BBUS_CLIENT_MON:
			r = client_list_add(cli, &monitors_head,
						&monitors_tail);
			if (r < 0) {
				logmsg(BBUS_LOG_ERR,
					"Error adding new monitor to "
					"the list: %s\n",
					bbus_strerror(bbus_lasterror()));
				continue;
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
}

static void handle_client(bbus_client* cli)
{
	int r;
	struct bbus_msg_hdr hdr;

	memset(msgbuf, 0, BBUS_MAXMSGSIZE);
	r = bbus_client_rcvmsg(cli, msgbuf, BBUS_MAXMSGSIZE);
	if (r < 0) {
		logmsg(BBUS_LOG_ERR,
			"Error receiving message from client: %s\n",
			bbus_strerror(bbus_lasterror()));
		return;
	}
	//send_to_monitors(msgbuf);

//	r = validate_msg(msgbuf);
//	if (r < 0) {
//		logmsg(BBUS_LOG_ERR,
//			"Invalid message received: %s\n",
//			bbus_strerror(bbus_lasterror()));
//		return;
//	}

	/* TODO Common function for error reporting. */
	memcpy(&hdr, msgbuf, sizeof(struct bbus_msg_hdr));
	switch (bbus_client_gettype(cli)) {
	case BBUS_CLIENT_CALLER:
		switch (hdr.msgtype) {
		case BBUS_MSGTYPE_CLICALL:
			r = handle_clientcall(cli, (struct bbus_msg*)msgbuf);
			if (r < 0) {
				logmsg(BBUS_LOG_ERR,
					"Error on client call: %s\n",
					bbus_strerror(bbus_lasterror()));
				return;
			}
			break;
		default:
			logmsg(BBUS_LOG_ERR, "Unexpected message received.\n");
			break;
		}
		break;
	case BBUS_CLIENT_SERVICE:
		switch (hdr.msgtype) {
		case BBUS_MSGTYPE_SRVREG:
			r = register_service(cli, (struct bbus_msg*)msgbuf);
			if (r < 0) {
				logmsg(BBUS_LOG_ERR,
					"Error registering a service: %s\n",
					bbus_strerror(bbus_lasterror()));
				return;
			}
			break;
		case BBUS_MSGTYPE_SRVUNREG:
			r = unregister_service(cli, (struct bbus_msg*)msgbuf);
			if (r < 0) {
				logmsg(BBUS_LOG_ERR,
					"Error unregistering a service: %s\n",
					bbus_strerror(bbus_lasterror()));
				return;
			}
			break;
		case BBUS_MSGTYPE_SRVREPLY:
			r = pass_srvc_reply(cli, (struct bbus_msg*)msgbuf);
			if (r < 0) {
				logmsg(BBUS_LOG_ERR,
					"Error passing a service reply: %s\n",
					bbus_strerror(bbus_lasterror()));
				return;
			}
			break;
		default:
			logmsg(BBUS_LOG_ERR, "Unexpected message received.\n");
			return;
		}
		break;
	case BBUS_CLIENT_CTL:
		switch (hdr.msgtype) {
		case BBUS_MSGTYPE_CTRL:
			handle_control_message(cli, (struct bbus_msg*)msgbuf);
			break;
		default:
			logmsg(BBUS_LOG_ERR, "Unexpected message received.\n");
			return;
		}
		break;
	case BBUS_CLIENT_MON:
		logmsg(BBUS_LOG_WARN, "Message received from a monitor which"
			" should not be sending any messages - discarding.\n");
		return;
		break;
	default:
		logmsg(BBUS_LOG_ERR,
			"Unhandled client type in the received message.\n");
		return;
	}
}

int main(int argc, char** argv)
{
	int retval;
	struct clientlist_elem* tmpcli;
	struct bbus_timeval tv;

	parse_args(argc, argv);
	if (options.print_help)
		print_help_and_exit();
	if (options.print_version)
		print_version_and_exit();

	/*
	 * Caller map:
	 * 	keys -> tokens,
	 * 	values -> pointers to caller objects.
	 */
	caller_map = bbus_hmap_create();
	if (caller_map == NULL) {
		die("Error creating the caller hashmap: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	/* Service map. */
	srvc_map = bbus_malloc(sizeof(struct service_map));
	if (srvc_map == NULL)
		goto err_map;

	srvc_map->subsrvc = bbus_hmap_create();
	if (srvc_map->subsrvc == NULL) {
		bbus_free(srvc_map);
		goto err_map;
	}

	srvc_map->methods = bbus_hmap_create();
	if (srvc_map->methods == NULL) {
		bbus_hmap_free(srvc_map->subsrvc);
		bbus_free(srvc_map);
		goto err_map;
	}

	/* Creating the server object. */
	server = bbus_srv_createp(sockpath);
	if (server == NULL) {
		die("Error creating the server object: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	retval = bbus_srv_listen(server);
	if (retval < 0) {
		die("Error opening server for connections: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	pollset = bbus_pollset_make();
	if (pollset == NULL) {
		die("Error creating the poll_set: %s\n",
			bbus_strerror(bbus_lasterror()));
	}

	logmsg(BBUS_LOG_INFO, "Busybus daemon starting!\n");
	run = 1;
	signal(SIGTERM, sighandler);
	signal(SIGINT, sighandler);

	/*
	 * MAIN LOOP
	 */
	while (do_run()) {
		memset(&tv, 0, sizeof(struct bbus_timeval));
		bbus_pollset_clear(pollset);
		bbus_pollset_addsrv(pollset, server);
		for (tmpcli = clients_head; tmpcli != NULL;
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
				die("Error polling connections: %s",
					bbus_strerror(bbus_lasterror()));
			}
		} else
		if (retval == 0) {
			// Timeout
			continue;
		} else {
			// Incoming data
			if (bbus_pollset_srvisset(pollset, server)) {
				accept_clients();
			}
			for (tmpcli = clients_head; tmpcli != NULL;
				tmpcli = tmpcli->next) {
				if (bbus_pollset_cliisset(pollset,
							tmpcli->cli)) {
					handle_client(tmpcli->cli);
				}
			}
		}
	}
	/*
	 * END OF THE MAIN LOOP
	 */

	/* TODO Cleanup. */

	logmsg(BBUS_LOG_INFO, "Busybus daemon exiting!\n");
	return 0;

err_map:
	die("Error creating the service map: %s\n",
		bbus_strerror(bbus_lasterror()));
}

