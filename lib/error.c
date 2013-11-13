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
#include <string.h>

/*
 * TODO: Implement thread specific error value using
 * pthread_setspecific() and use strerror_r for thread-safety.
 */

static volatile int last_error = BBUS_ESUCCESS;

static const char* const error_descr[] = {
	"success",
	"out of memory",
	"invalid argument",
	"invalid busybus object format",
	"not enough space in buffer",
	"connection closed by remote peer",
	"invalid message format",
	"wrong magic number in received message",
	"received message of incorrect type",
	"session open rejected",
	"didn't manage to send all data",
	"received less data, than expected",
	"internal logic error",
	"no such method",
	"internal method error",
	"poll interrupted by a signal",
	"error registering the method",
	"invalid key type used on a hashmap"
};

int bbus_lasterror(void)
{
	return last_error;
}

const char* bbus_strerror(int errnum)
{
	if (errnum < BBUS_ESUCCESS)
		return strerror(errnum);
	else if (errnum >= __BBUS_MAX_ERR)
		return "invalid error code";
	else
		return error_descr[errnum-BBUS_ESUCCESS];
}

void __bbus_seterr(int errnum)
{
	last_error = errnum;
}

