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

#include "busybus.h"
#include "protocol.h"
#include "socket.h"
#include "error.h"
#include <string.h>

struct __bbus_client_connection
{
	int sock;
};

struct __bbus_service_connection
{
	int sock;
	char* srvname;
	bbus_hashmap* methods;
};

static int do_session_open(const char* path, int sotype)
{
	int r;
	struct bbus_msg_hdr hdr;
	int sock;

	sock = __bbus_local_socket();
	if (sock < 0)
		goto errout;

	r = __bbus_local_connect(sock, path);
	if (r < 0)
		goto errout;

	memset(&hdr, 0, BBUS_MSGHDR_SIZE);
	__bbus_hdr_setmagic(&hdr);
	hdr.msgtype = sotype;
	r = __bbus_sendv_msg(sock, &hdr, NULL, NULL, 0);
	if (r < 0)
		goto errout_close;

	memset(&hdr, 0, BBUS_MSGHDR_SIZE);
	r = __bbus_recvv_msg(sock, &hdr, NULL, 0);
	if (r < 0)
		goto errout_close;

	if (hdr.msgtype == BBUS_MSGTYPE_SOOK) {
		return sock;
	} else
	if (hdr.msgtype == BBUS_MSGTYPE_SORJCT) {
		__bbus_set_err(BBUS_SORJCTD);
		goto errout_close;
	} else {
		__bbus_set_err(BBUS_MSGINVTYPERCVD);
		goto errout_close;
	}

errout_close:
	__bbus_sock_close(sock);
errout:
	return -1;
}

static int send_session_close(int sock)
{
	int r;
	struct bbus_msg_hdr hdr;

	memset(&hdr, 0, BBUS_MSGHDR_SIZE);
	__bbus_hdr_setmagic(&hdr);
	hdr.msgtype = BBUS_MSGTYPE_CLOSE;
	r = __bbus_sendv_msg(sock, &hdr, NULL, NULL, 0);
	if (r < 0)
		return -1;
	r = __bbus_sock_close(sock);
	if (r < 0)
		return -1;
	return 0;
}

static const char* extract_meta(const void* buf, size_t bufsize)
{
	char* ptr;

	ptr = memmem(buf, bufsize, "\0", 1);
	if (ptr == NULL)
		return NULL;
	return buf;
}

static bbus_object* extract_object(const void* buf, size_t bufsize)
{
	const void* ptr;
	bbus_object* obj;

	ptr = extract_meta(buf, bufsize);
	if (ptr == NULL)
		return NULL;
	buf += (int)(ptr - buf);
	bufsize -= (int)(ptr - buf);
	obj = bbus_object_from_buf(buf, bufsize);
	if (obj == NULL)
		return NULL;

	return obj;
}

bbus_client_connection* bbus_client_connect(void)
{
	return bbus_client_connect_wpath(BBUS_DEF_DIRPATH BBUS_DEF_SOCKNAME);
}

bbus_client_connection* bbus_client_connect_wpath(const char* path)
{
	int sock;
	bbus_client_connection* conn;

	sock = do_session_open(path, BBUS_MSGTYPE_SOCLI);
	if (sock < 0)
		return NULL;

	conn = bbus_malloc(sizeof(struct __bbus_client_connection));
	if (conn == NULL)
		return NULL;
	conn->sock = sock;
	return conn;
}

bbus_object* bbus_call_method(bbus_client_connection* conn,
		char* method, bbus_object* arg)
{
	int r;
	struct bbus_msg_hdr hdr;
	size_t metasize;
	size_t objsize;
	char buf[BBUS_MAXMSGSIZE];

	metasize = strlen(method) + 1;
	objsize = bbus_obj_rawdata_size(arg);
	memset(&hdr, 0, sizeof(struct bbus_msg_hdr));
	__bbus_hdr_setmagic(&hdr);
	hdr.msgtype = BBUS_MSGTYPE_CLICALL;
	hdr.psize = metasize + objsize;
	hdr.flags |= BBUS_PROT_HASMETA;
	hdr.flags |= BBUS_PROT_HASOBJECT;

	r = __bbus_sendv_msg(conn->sock, &hdr, method,
			bbus_obj_rawdata(arg), objsize);
	if (r < 0)
		return NULL;

	memset(&hdr, 0, BBUS_MSGHDR_SIZE);
	memset(buf, 0, BBUS_MAXMSGSIZE);
	r = __bbus_recvv_msg(conn->sock, &hdr, buf,
			BBUS_MAXMSGSIZE - BBUS_MSGHDR_SIZE);
	if (r < 0)
		return NULL;

	if (hdr.msgtype == BBUS_MSGTYPE_CLIREPLY) {
		if (hdr.errcode != 0) {
			__bbus_set_err(__bbus_proterr_to_errnum(hdr.errcode));
			return NULL;
		}
		return bbus_object_from_buf(buf, BBUS_MAXMSGSIZE);
	} else {
		__bbus_set_err(BBUS_MSGINVTYPERCVD);
		return NULL;
	}
}

int bbus_close_client_conn(bbus_client_connection* conn)
{
	int r;

	r = send_session_close(conn->sock);
	if (r < 0)
		return -1;
	bbus_free(conn);

	return 0;
}

bbus_service_connection* bbus_service_connect(const char* name)
{
	return bbus_service_connect_wpath(name,
			BBUS_DEF_DIRPATH BBUS_DEF_SOCKNAME);
}

bbus_service_connection* bbus_service_connect_wpath(const char* name,
		const char* path)
{
	int sock;
	bbus_service_connection* conn;

	sock = do_session_open(path, BBUS_MSGTYPE_SOSRVP);
	if (sock < 0)
		return NULL;

	conn = bbus_malloc(sizeof(struct __bbus_service_connection));
	if (conn == NULL)
		return NULL;
	conn->sock = sock;
	conn->srvname = bbus_copy_string(name);
	conn->methods = bbus_hmap_create();
	if (conn->methods == NULL) {
		__bbus_sock_close(conn->sock);
		bbus_free_string(conn->srvname);
		bbus_free(conn);
		return NULL;
	}
	return conn;
}

int bbus_register_method(bbus_service_connection* conn,
		struct bbus_method* method)
{
	struct bbus_msg_hdr hdr;
	size_t metasize;
	char* meta;
	int r;

	metasize = strlen(conn->srvname);
	metasize += strlen(method->name) + 1; /* +1 for comma */
	metasize += strlen(method->argdscr) + 1; /* +1 for comma */
	metasize += strlen(method->retdscr) + 1; /* +1 for NULL */
	memset(&hdr, 0, sizeof(struct bbus_msg_hdr));
	__bbus_hdr_setmagic(&hdr);
	hdr.msgtype = BBUS_MSGTYPE_SRVREG;
	hdr.psize = metasize;
	meta = bbus_build_string("%s%s,%s,%s",
					conn->srvname,
					method->name,
					method->argdscr,
					method->retdscr);
	if (meta == NULL) {
		return -1;
	} else
	if (strlen(meta) != (metasize-1)) {
		bbus_free_string(meta);
		__bbus_set_err(BBUS_LOGICERR);
		return -1;
	}

	r = __bbus_sendv_msg(conn->sock, &hdr, meta, NULL, 0);
	if (r < 0)
		return -1;

	memset(&hdr, 0, BBUS_MSGHDR_SIZE);
	r = __bbus_recvv_msg(conn->sock, &hdr, NULL, 0);
	if (r < 0)
		return -1;

	if (hdr.msgtype != BBUS_MSGTYPE_SRVACK) {
		__bbus_set_err(BBUS_MSGINVTYPERCVD);
		return -1;
	}
	if (hdr.errcode != 0) {
		__bbus_set_err(__bbus_proterr_to_errnum(hdr.errcode));
		return -1;
	}

	r = bbus_hmap_insert(conn->methods, method->name, (void*)method->func);
	if (r < 0)
		return -1;

	return 0;
}

int bbus_listen_method_calls(bbus_service_connection* conn,
		struct bbus_timeval* tv)
{
	int r;
	struct bbus_msg_hdr hdr;
	char buf[BBUS_MAXMSGSIZE];
	const char* meta;
	bbus_object* objarg;
	bbus_object* objret;
	void* callback;
	uint32_t token;

	r = __bbus_sock_rd_ready(conn->sock, tv);
	if (r < 0) {
		return -1;
	} else
	if (r == 0) {
		/* Timeout */
		return 0;
	} else {
		/* Message incoming */
		memset(&hdr, 0, BBUS_MSGHDR_SIZE);
		memset(buf, 0, BBUS_MAXMSGSIZE);
		r = __bbus_recvv_msg(conn->sock, &hdr, buf, BBUS_MAXMSGSIZE);
		if (r < 0)
			return -1;
		if (hdr.msgtype != BBUS_MSGTYPE_SRVCALL) {
			__bbus_set_err(BBUS_MSGINVTYPERCVD);
			return -1;
		}

		token = hdr.token;
		meta = extract_meta(buf, BBUS_MAXMSGSIZE);
		if (meta == NULL) {
			__bbus_set_err(BBUS_MSGINVFMT);
			return -1;
		}

		objarg = extract_object(buf, BBUS_MAXMSGSIZE);
		if (objarg == NULL) {
			__bbus_set_err(BBUS_MSGINVFMT);
			return -1;
		}

		memset(&hdr, 0, sizeof(struct bbus_msg_hdr));
		__bbus_hdr_setmagic(&hdr);
		hdr.msgtype = BBUS_MSGTYPE_SRVREPLY;
		hdr.token = token;
		objret = NULL;
		callback = bbus_hmap_find(conn->methods, meta);
		if (callback == NULL) {
			hdr.errcode = BBUS_PROT_NOMETHOD;
			__bbus_set_err(BBUS_NOMETHOD);
			goto send_reply;
		}

		objret = ((bbus_method_func)callback)(meta, objarg);
		if (objret == NULL) {
			hdr.errcode = BBUS_PROT_METHODERR;
			__bbus_set_err(BBUS_METHODERR);
			goto send_reply;
		}

send_reply:
		r = __bbus_sendv_msg(conn->sock, &hdr, NULL,
			objret == NULL ? NULL : bbus_obj_rawdata(objret),
			objret == NULL ? 0 : bbus_obj_rawdata_size(objret));
		if (r < 0)
			return -1;

		bbus_free_object(objret);
		bbus_free_object(objarg);
	}

	return hdr.errcode == 0 ? 0 : -1;
}

int bbus_close_service_conn(bbus_service_connection* conn)
{
	int r;

	r = send_session_close(conn->sock);
	if (r < 0)
		return -1;
	bbus_free_string(conn->srvname);
	bbus_hmap_free(conn->methods);
	bbus_free(conn);
	return 0;
}

