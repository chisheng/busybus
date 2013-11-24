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

#include "bbus-unit.h"
#include <busybus.h>
#include <string.h>
#include <stdint.h>

BBUSUNIT_DEFINE_TEST(prot_extract_obj)
{
	BBUSUNIT_BEGINTEST;

		static const char msgbuf[] =
				"\xBB\xC5"
				"\x01"
				"\x00"
				"\x00\x00\x00\x00"
				"\x00\x09"
				"\x02"
				"\x00"
				"a string\0";

		static const struct bbus_msg* msg = (struct bbus_msg*)msgbuf;
		static const size_t objsize = sizeof(msgbuf)-1
							-BBUS_MSGHDR_SIZE;

		bbus_object* obj;
		char* s;
		int ret;

		obj = bbus_prot_extractobj(msg);
		BBUSUNIT_ASSERT_NOTNULL(obj);
		BBUSUNIT_ASSERT_EQ(objsize, bbus_obj_rawsize(obj));
		ret = bbus_obj_extrstr(obj, &s);
		BBUSUNIT_ASSERT_EQ(0, ret);
		BBUSUNIT_ASSERT_STREQ("a string", s);

	BBUSUNIT_FINALLY;

		bbus_obj_free(obj);

	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_extract_meta)
{
	BBUSUNIT_BEGINTEST;

		static const char msgbuf[] =
				"\xBB\xC5"
				"\x01"
				"\x00"
				"\x00\x00\x00\x00"
				"\x00\x0C"
				"\x01"
				"\x00"
				"meta string\0";

		static const struct bbus_msg* msg = (struct bbus_msg*)msgbuf;

		const char* meta;

		meta = bbus_prot_extractmeta(msg);
		BBUSUNIT_ASSERT_NOTNULL(meta);
		BBUSUNIT_ASSERT_STREQ("meta string", meta);

	BBUSUNIT_FINALLY;
	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_extract_meta_and_obj)
{
	BBUSUNIT_BEGINTEST;

		static const char msgbuf[] =
				"\xBB\xC5"
				"\x01"
				"\x00"
				"\x00\x00\x00\x00"
				"\x00\x14"
				"\x03"
				"\x00"
				"meta string\0"
				"\x11\x22\x33\x44"
				"\x55\x66\x77\x88";

		static const struct bbus_msg* msg = (struct bbus_msg*)msgbuf;

		bbus_object* obj = NULL;
		/* FIXME GCC complains later if obj is uninitialized, why? */
		const char* meta;

		meta = bbus_prot_extractmeta(msg);
		BBUSUNIT_ASSERT_NOTNULL(meta);
		BBUSUNIT_ASSERT_STREQ("meta string", meta);
		obj = bbus_prot_extractobj(msg);
		BBUSUNIT_ASSERT_NOTNULL(obj);
		BBUSUNIT_ASSERT_EQ(2*sizeof(bbus_uint32),
					bbus_obj_rawsize(obj));
		BBUSUNIT_ASSERT_EQ(0, memcmp(
					"\x11\x22\x33\x44\x55\x66\x77\x88",
					bbus_obj_rawdata(obj),
					bbus_obj_rawsize(obj)));

	BBUSUNIT_FINALLY;

		bbus_obj_free(obj);

	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_extract_invalid_meta)
{
	BBUSUNIT_BEGINTEST;

		static const char msgbuf[] =
				"\xBB\xC5"
				"\x01"
				"\x00"
				"\x00\x00\x00\x00"
				"\x00\x0C"
				"\x01"
				"\x00"
				"meta string without null";

		static const struct bbus_msg* msg = (struct bbus_msg*)msgbuf;

		const char* meta;

		meta = bbus_prot_extractmeta(msg);
		BBUSUNIT_ASSERT_NULL(meta);

	BBUSUNIT_FINALLY;
	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_extract_flags_not_set)
{
	BBUSUNIT_BEGINTEST;

		static const char msgbuf[] =
				"\xBB\xC5"
				"\x01"
				"\x00"
				"\x00\x00\x00\x00"
				"\x00\x14"
				"\x00"
				"\x00"
				"meta string\0"
				"\x11\x22\x33\x44"
				"\x55\x66\x77\x88";

		static const struct bbus_msg* msg = (struct bbus_msg*)msgbuf;

		const char* meta;
		bbus_object* obj;

		meta = bbus_prot_extractmeta(msg);
		BBUSUNIT_ASSERT_NULL(meta);
		obj = bbus_prot_extractobj(msg);
		BBUSUNIT_ASSERT_NULL(obj);

	BBUSUNIT_FINALLY;
	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_set_and_get_path)
{
	BBUSUNIT_BEGINTEST;

		static const char newpath[] = "/tmp/newsock.sock";

		BBUSUNIT_ASSERT_STREQ(BBUS_PROT_DEFSOCKPATH,
						bbus_prot_getsockpath());
		bbus_prot_setsockpath(newpath);
		BBUSUNIT_ASSERT_STREQ(newpath, bbus_prot_getsockpath());

	BBUSUNIT_FINALLY;
	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_set_psize)
{
	BBUSUNIT_BEGINTEST;

		static const size_t size = 1024;

		struct bbus_msg_hdr hdr;

		memset(&hdr, 0, sizeof(struct bbus_msg_hdr));
		bbus_hdr_setpsize(&hdr, size);
		BBUSUNIT_ASSERT_EQ(size, bbus_hdr_getpsize(&hdr));

	BBUSUNIT_FINALLY;
	BBUSUNIT_ENDTEST;
}

BBUSUNIT_DEFINE_TEST(prot_set_psize_gtmax)
{
	BBUSUNIT_BEGINTEST;

		static const size_t size = 2 * UINT16_MAX;

		struct bbus_msg_hdr hdr;

		memset(&hdr, 0, sizeof(struct bbus_msg_hdr));
		bbus_hdr_setpsize(&hdr, size);
		BBUSUNIT_ASSERT_EQ(UINT16_MAX, bbus_hdr_getpsize(&hdr));

	BBUSUNIT_FINALLY;
	BBUSUNIT_ENDTEST;
}

