/*
 *
  Copyright (c) Dialogic (R) 2009 - 2010
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
#ifndef __DIVA_OS_STREAMING_PLATFORM_H__
#define __DIVA_OS_STREAMING_PLATFORM_H__

#define DIVA_USERMODE 1
#define LINUX         1

#if !defined(__i386__)
#define READ_WORD(w) ( ((byte *)(w))[0] + \
                      (((byte *)(w))[1]<<8) )

#define READ_DWORD(w) ( ((byte *)(w))[0] + \
                       (((byte *)(w))[1]<<8) + \
                       (((byte *)(w))[2]<<16) + \
                       (((byte *)(w))[3]<<24) )

#define WRITE_WORD(b,w) do{ ((byte*)(b))[0]=(byte)(w); \
                          ((byte*)(b))[1]=(byte)((w)>>8); }while(0)

#define WRITE_DWORD(b,w) do{ ((byte*)(b))[0]=(byte)(w); \
                           ((byte*)(b))[1]=(byte)((w)>>8); \
                           ((byte*)(b))[2]=(byte)((w)>>16); \
                           ((byte*)(b))[3]=(byte)((w)>>24); }while(0)
#else
#define READ_WORD(w) (*(word *)(w))
#define READ_DWORD(w) (*(dword *)(w))
#define WRITE_WORD(b,w) do{ *(word *)(b)=(w); }while(0)
#define WRITE_DWORD(b,w) do{ *(dword *)(b)=(w); }while(0)
#endif

typedef unsigned char      byte;
typedef unsigned short     word;
typedef unsigned int       dword;
typedef signed int         int32;
typedef unsigned long long qword;

#ifndef likely
#define likely(__x__)   (!!(__x__))
#endif
#ifndef unlikely
#define unlikely(__x__) (!!(__x__))
#endif

#ifndef MIN
#define MIN(a,b)  ((a)>(b) ? (b) : (a))
#endif

#ifndef MAX
#define MAX(a,b)  ((a)>(b) ? (a) : (b))
#endif

#define DIVAS_CONTAINING_RECORD(address, type, field) \
        ((type *)((char*)(address) - (char*)(&((type *)0)->field)))

#define DIVA_STREAMING_MAX_TRACE_IDENT_LENGTH 4

void* diva_os_malloc (unsigned long flags, unsigned long size);
void diva_os_free (unsigned long flags, void* ptr);

#define _DI_STREAM_CTRL 0x0014

#include <string.h>
#include "debuglib.h"

#define DIVA_SHARED_SEGMENT_ALLOC 1
/*
  DIVA_SHARED_SEGMENT_LOCK used to lock shared segment alloc instance. Locked instance is newer freed
  due to reference counter newer receives zero. This allows to maintain permenently mapped pool of pages
  and to reduce overall system load.
  Causes memory load if application is stopped, but memory and mapped resources are freed by OS.
  */
#define DIVA_SHARED_SEGMENT_LOCK  1

#endif

