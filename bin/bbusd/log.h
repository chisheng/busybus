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
 */

#ifndef __BBUSD_LOG__
#define __BBUSD_LOG__

#include <busybus.h>
#include <syslog.h>

enum bbusd_loglevel
{
	BBUSD_LOG_EMERG = LOG_EMERG,
	BBUSD_LOG_ALERT = LOG_ALERT,
	BBUSD_LOG_CRIT = LOG_CRIT,
	BBUSD_LOG_ERR = LOG_ERR,
	BBUSD_LOG_WARN = LOG_WARNING,
	BBUSD_LOG_NOTICE = LOG_NOTICE,
	BBUSD_LOG_INFO = LOG_INFO,
	BBUSD_LOG_DEBUG = LOG_DEBUG
};

void bbusd_logmsg(enum bbusd_loglevel lvl, const char* fmt, ...)
						BBUS_PRINTF_FUNC(2, 3);

#endif /* __BBUSD_LOG__ */

