/*
 *
  Copyright (c) Dialogic (R) 2009 - 2011
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Dialogic (R) File Revision :    1.9
 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#include <stdio.h>
#include "platform.h"
#include <malloc.h>
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_utils.h"

void* diva_os_malloc (unsigned long flags, unsigned long size) {
	void* ret = 0;

	if (size != 0) {
		ret = ast_malloc (size);
	}

	return (ret);
}

void diva_os_free (unsigned long flags, void* ptr) {
	if (ptr != 0) {
		ast_free (ptr);
	}
}

void diva_runtime_error_message (const char* fmt, ...) {
	char tmp[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	tmp[sizeof(tmp)-1]=0;

	cc_log(LOG_ERROR, "%s\n", tmp);
}

void diva_runtime_log_message (const char* fmt, ...) {
	char tmp[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	tmp[sizeof(tmp)-1]=0;

	cc_verbose(4, 0, "%s\n", tmp);
}

void diva_runtime_trace_message (const char* fmt, ...) {
	char tmp[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	tmp[sizeof(tmp)-1]=0;

	cc_verbose(4, 1, "%s\n", tmp);
}

void diva_runtime_binary_message (const void* data, unsigned long data_length) {
	static const char hex_digit_table[0x10] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  word i, j;
  char *p;
  char hex_line[50];
  const byte *buffer = data;
	word length = (word)data_length;

	for (i = 0; i < length; i += 16) {
		p = hex_line;
		for (j = 0; (j < 16) && (i+j < length); j++) {
			*(p++) = ' ';
			*(p++) = hex_digit_table[buffer[i+j] >> 4];
			*(p++) = hex_digit_table[buffer[i+j] & 0xf];
		}
		*p = '\0';

		cc_verbose(4, 1, "[%02x]%s\n", (unsigned int) i, hex_line);
  }
}

