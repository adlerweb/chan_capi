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
#ifndef __DIVA_SEGMENT_ALLOC_H__
#define __DIVA_SEGMENT_ALLOC_H__

struct _diva_segment_alloc;
struct _diva_segment_alloc_access;

typedef struct _diva_segment_alloc_access {
	void  (*release)(struct _diva_segment_alloc** ifc);
	void* (*segment_alloc)(struct _diva_segment_alloc* ifc, dword* lo, dword* hi);
	void  (*segment_free)(struct _diva_segment_alloc* ifc, void* addr, dword lo, dword hi);
	dword (*get_segment_length)(struct _diva_segment_alloc* ifc);
	void* (*map_address)(struct _diva_segment_alloc* ifc, dword lo, dword hi, int map_host);
	void* (*umap_address)(struct _diva_segment_alloc* ifc, dword lo, dword hi, void* local);
	int   (*write_address)(struct _diva_segment_alloc* ifc, dword lo, dword hi, dword data);
	void  (*resource_removed)(struct _diva_segment_alloc* ifc);
} diva_segment_alloc_access_t;

int diva_create_segment_alloc  (void* os_context, struct _diva_segment_alloc** segment_alloc);
int diva_destroy_segment_alloc (struct _diva_segment_alloc** segment_alloc);
diva_segment_alloc_access_t* diva_get_segment_alloc_ifc (struct _diva_segment_alloc* segment_alloc);

#endif
