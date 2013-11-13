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

#ifndef __BBUS_SOCKET__
#define __BBUS_SOCKET__

#include <busybus.h>
#include <stdlib.h>
#include <sys/uio.h>

int __bbus_sock_mksocket(void);
int __bbus_sock_bind(int sock, const char* path);
int __bbus_sock_listen(int sock, int backlog);
int __bbus_sock_accept(int sock, char* pathbuf,
		size_t bufsize, size_t* pathsize);
int __bbus_sock_connect(int sock, const char* path);
int __bbus_sock_close(int sock);
int __bbus_sock_rm(const char* path);
ssize_t __bbus_sock_send(int sock, const void* buf, size_t size);
ssize_t __bbus_sock_recv(int sock, void* buf, size_t size);
ssize_t __bbus_sock_sendv(int sock, const struct iovec* iov, int numiov);
ssize_t __bbus_sock_recvv(int sock, struct iovec* iov, int numiov);
int __bbus_sock_wrready(int sock, struct bbus_timeval* tv);
int __bbus_sock_rdready(int sock, struct bbus_timeval* tv);

#endif /* __BBUS_SOCKET__ */
