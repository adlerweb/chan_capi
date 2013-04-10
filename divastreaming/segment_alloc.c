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
/*
 * vim:ts=2:
 */
#include "platform.h"
#include "dlist.h"
#include "diva_segment_alloc_ifc.h"
#ifdef DIVA_USERMODE
#include "xdi_msg.h"
#else
#include "pc.h"
#include "di_defs.h"
#include "divasync.h"
#endif
#include "debuglib.h"

#if defined(LINUX) && defined(DIVA_USERMODE)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

typedef struct _diva_map_entry {
	diva_entity_link_t link;
	int  entry_nr;
	dword dma_lo;
	dword dma_hi;
	void* mem;
} diva_map_entry_t;

typedef struct _diva_segment_alloc {
	diva_segment_alloc_access_t ifc;
#if defined(DIVA_USERMODE)
#if defined(LINUX)
	int fd;
	int fd_mem;
	int fd_xdi;
#endif
#else
	DESCRIPTOR* d;
#endif
	diva_entity_queue_t free_q;
	diva_entity_queue_t busy_q;
#if !defined(DIVA_USERMODE)
	IDI_SYNC_REQ syncReq;
#endif
} diva_segment_alloc_t;

/*
 * LOCALS
 */
static void  release_proc(struct _diva_segment_alloc**);
static void* segment_alloc_proc(struct _diva_segment_alloc*, dword* lo, dword* hi);
static void  segment_free_proc(struct _diva_segment_alloc*, void* addr, dword lo, dword hi);
static dword get_segment_length_proc(struct _diva_segment_alloc*);
#if defined(DIVA_USERMODE)
static int map_entry (struct _diva_segment_alloc* pI, diva_map_entry_t* pE);
#endif
static void* map_address (struct _diva_segment_alloc* ifc, dword lo, dword hi, int map_host);
static void* umap_address (struct _diva_segment_alloc* ifc, dword lo, dword hi, void* local);
static int   write_address (struct _diva_segment_alloc* ifc, dword lo, dword hi, dword data);
static void  resource_removed (struct _diva_segment_alloc* ifc);

#if !defined(DIVA_USERMODE)
static void diva_segment_allloc_resource_removed_request(ENTITY* e) { }
static DESCRIPTOR diva_segment_alloc_resource_removed_descriptor =
									{ 0, 0, 0, diva_segment_allloc_resource_removed_request };
#endif

static diva_segment_alloc_access_t ifc_ref = {
  release_proc,
  segment_alloc_proc,
  segment_free_proc,
  get_segment_length_proc,
	map_address,
	umap_address,
	write_address,
	resource_removed
};

#if defined(DIVA_SHARED_SEGMENT_ALLOC)
static struct _diva_segment_alloc* shared_segment_alloc;
static int shared_segment_alloc_count;
#endif

int diva_create_segment_alloc  (void* os_context, struct _diva_segment_alloc** segment_alloc)
{
	diva_segment_alloc_t* pI = diva_os_malloc(0, sizeof(*pI));

#if defined(DIVA_SHARED_SEGMENT_ALLOC)
	if (shared_segment_alloc != 0) {
		shared_segment_alloc_count++;
		*segment_alloc = shared_segment_alloc;
		DBG_TRC(("shared %d segment alloc [%p]", shared_segment_alloc_count, pI))
		return (0);
	}
#endif

	pI = diva_os_malloc(0, sizeof(*pI));

	if (pI != 0) {
		memset (pI, 0x00, sizeof(*pI));

		pI->ifc = ifc_ref;

		diva_q_init (&pI->free_q);
		diva_q_init (&pI->busy_q);

#if defined(DIVA_USERMODE) /* { */
#if defined(LINUX) /* { */
		pI->fd = open ("/dev/DivasMAP", O_RDWR | O_NONBLOCK); /** \todo use hardware related DMA entry, needs update of XDI driver */
    pI->fd_mem = open ("/dev/mem", O_RDWR | O_NONBLOCK); /* /dev/mem is optional */
    pI->fd_xdi = open ("/dev/DivasXDI", O_RDWR | O_NONBLOCK); /* /dev/mem is optional */

		if (pI->fd >= 0 && (pI->fd_mem >= 0 || pI->fd_xdi >= 0)) {
			*segment_alloc = pI;
		} else {
			diva_destroy_segment_alloc (&pI);
			pI = 0;
		}
#endif /* } */
#else /* } { */
		pI->d = (DESCRIPTOR*)os_context;
		*segment_alloc = pI;
#endif /* } */
	}

	if (pI != 0) {
#if defined(DIVA_SHARED_SEGMENT_ALLOC)
		shared_segment_alloc = pI;
		shared_segment_alloc_count = 1;
#if defined(DIVA_SHARED_SEGMENT_LOCK)
		shared_segment_alloc_count++;
#endif
#endif
		DBG_TRC(("created segment alloc [%p]", pI))
	}

	return ((pI != 0) ? 0 : -1);
}

int diva_destroy_segment_alloc (struct _diva_segment_alloc** segment_alloc) {
	diva_segment_alloc_t* pI = (segment_alloc != 0) ? *segment_alloc : 0;

#if defined(DIVA_SHARED_SEGMENT_ALLOC)
	shared_segment_alloc_count--;
	if (shared_segment_alloc_count > 0) {
		if (segment_alloc != 0)
			*segment_alloc = 0;
		DBG_TRC(("unshare %d segment alloc [%p]", shared_segment_alloc_count, segment_alloc))
		return (0);
	}
#endif

	DBG_TRC(("destroy segment alloc [%p]", segment_alloc))

	if (pI != 0) {
		diva_entity_link_t* link;

		shared_segment_alloc = 0;
		shared_segment_alloc_count = 0;

		while ((link = diva_q_get_head (&pI->busy_q)) != 0) {
			diva_q_remove (&pI->busy_q, link);
			diva_q_add_tail (&pI->free_q, link);
		}

		while ((link = diva_q_get_head (&pI->free_q)) != 0) {
			diva_map_entry_t* pE = DIVAS_CONTAINING_RECORD(link, diva_map_entry_t, link);
#if defined(DIVA_USERMODE)
#if defined(LINUX)
			munmap (pE->mem, 4*1024);
#endif
#else
		pI->syncReq.diva_xdi_streaming_mapping_req.Req = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.Rc  = IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.request = IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING_FREE_COMMAND;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_lo     = pE->dma_lo;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_hi     = pE->dma_hi;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.addr       = pE->mem;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_handle = pE->entry_nr;
		pI->d->request((ENTITY*)&pI->syncReq);
#endif
			diva_q_remove (&pI->free_q, link);
			diva_os_free (0, pE);
		}

#if defined(DIVA_USERMODE)
#if defined(LINUX)
		if (pI->fd >= 0)
			close (pI->fd);
		if (pI->fd_mem >= 0)
			close (pI->fd_mem);
		if (pI->fd_xdi >= 0)
			close (pI->fd_xdi);
#endif
#else

#endif


		diva_os_free (0, pI);

		*segment_alloc = 0;
	}

	return (0);
}

void  release_proc(struct _diva_segment_alloc** pI) {
	diva_destroy_segment_alloc (pI);
}

void* segment_alloc_proc(struct _diva_segment_alloc* pI, dword* lo, dword* hi) {
	diva_entity_link_t* link = diva_q_get_head(&pI->free_q);
	void* addr = 0;

	if (link != 0) {
		diva_map_entry_t* pE = DIVAS_CONTAINING_RECORD(link, diva_map_entry_t, link);

		diva_q_remove (&pI->free_q, link);
		diva_q_add_tail (&pI->busy_q, link);

		*lo = pE->dma_lo;
		*hi = pE->dma_hi;

		return (pE->mem);
	} else if ((link = diva_os_malloc (0, sizeof(diva_map_entry_t))) != 0) {
		diva_map_entry_t* pE = (diva_map_entry_t*)link;
#if defined(DIVA_USERMODE)
#if defined(LINUX)
		dword data[5];
		int ret;
    
		data[0] = DIVA_XDI_UM_CMD_CREATE_XDI_DESCRIPTORS;
		data[1] = 1;

    { int tmp = write (pI->fd, data, 2*sizeof(dword)); tmp++; }
    ret = read (pI->fd, data, sizeof(data));
    if (ret == sizeof(data) || ret == (sizeof(data)-sizeof(data[0]))) {
			if (data[0] == DIVA_XDI_UM_CMD_CREATE_XDI_DESCRIPTORS && data[1] == 1) {
				pE->dma_lo = data[3];
				pE->dma_hi = (data[2] == 8) ? data[4] : 0;
				if (map_entry(pI, pE) == 0) {
					diva_q_add_tail (&pI->busy_q, &pE->link);
					*lo  = pE->dma_lo;
					*hi  = pE->dma_hi;
					addr = pE->mem;
				} else {
					diva_os_free (0, pE);
				}
      }
		}
#endif
#else
		pI->syncReq.diva_xdi_streaming_mapping_req.Req = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.Rc  = IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.request = IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING_ALLOC_COMMAND;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_lo     = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_hi     = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.addr       = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_handle = -1;
		pI->d->request((ENTITY*)&pI->syncReq);
		if (pI->syncReq.diva_xdi_streaming_mapping_req.info.request == IDI_SYNC_REQ_PROCESS_STREAMING_COMMAND_OK &&
				pI->syncReq.diva_xdi_streaming_mapping_req.info.addr != 0) {
			pE->entry_nr = pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_handle;
			pE->dma_lo   = pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_lo;
			pE->dma_hi   = pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_hi;
			pE->mem      = pI->syncReq.diva_xdi_streaming_mapping_req.info.addr;

			*lo  = pE->dma_lo;
			*hi  = pE->dma_hi;
			addr = pE->mem;

			memset (addr, 0x00, 4*1024);

			diva_q_add_tail (&pI->busy_q, &pE->link);
		} else {
			diva_os_free (0, pE);
		}
#endif
	}

	return (addr);
}

#if defined(DIVA_USERMODE)
static int map_entry (struct _diva_segment_alloc* pI, diva_map_entry_t* pE) {
	void* addr;

	if (pE->dma_hi != 0) {
		qword i = ((qword)pE->dma_lo) | (((qword)pE->dma_hi) << 32);
#if defined(LINUX)
		addr = mmap (0, 4*1024, PROT_READ|PROT_WRITE, MAP_SHARED, pI->fd, i);
#endif
	} else {
#if defined(LINUX)
		addr = mmap (0, 4*1024, PROT_READ|PROT_WRITE, MAP_SHARED, pI->fd, pE->dma_lo);
#endif
	}
	if (addr == 0 || addr == ((void*)-1)) {
		DBG_ERR(("failed to map %08x:%08x [%p]", pE->dma_lo, pE->dma_hi, pI))

		return (-1);
	}

	pE->mem = addr;

	return (0);
}
#endif

static void* map_address (struct _diva_segment_alloc* pI, dword lo, dword hi, int map_host) {
	void* ret = 0;

#if defined(DIVA_USERMODE)
#if defined(LINUX)
	qword addr = ((qword)lo) | (((qword)hi) << 32);
	int effective_map_fd = (map_host == 0) ? pI->fd_mem : pI->fd;

	if (effective_map_fd >= 0) {
		ret = mmap (0, 4*1024, PROT_READ|PROT_WRITE, MAP_SHARED, effective_map_fd, addr);
		if (ret == ((void*)-1)) {
			ret = 0;
		}
	}
#endif
#else
	pI->syncReq.diva_xdi_streaming_mapping_req.Req = 0;
	pI->syncReq.diva_xdi_streaming_mapping_req.Rc  = IDI_SYNC_REQ_PROCESS_STREAMING_MAPPING;
	pI->syncReq.diva_xdi_streaming_mapping_req.info.request = IDI_SYNC_REQ_PROCESS_STREAMING_SYSTEM_MAP_COMMAND;
	pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_lo     = 0;
	pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_hi     = 0;
	pI->syncReq.diva_xdi_streaming_mapping_req.info.addr       = 0;
	pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_handle = -1;
	pI->d->request((ENTITY*)&pI->syncReq);
	if (pI->syncReq.diva_xdi_streaming_mapping_req.info.request == IDI_SYNC_REQ_PROCESS_STREAMING_COMMAND_OK) {
		byte* p = pI->syncReq.diva_xdi_streaming_mapping_req.info.addr;
		dword offset = lo - pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_lo;

		pI->syncReq.diva_xdi_streaming_mapping_req.info.addr = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_lo = 0;
		pI->syncReq.diva_xdi_streaming_mapping_req.info.dma_hi = 0;

		ret = p + offset;
	}
#endif

	return (ret);
}

static void* umap_address (struct _diva_segment_alloc* ifc, dword lo, dword hi, void* local) {
#if defined(DIVA_USERMODE)
#if defined(LINUX)
	munmap (local, 4*1024);
#endif
#else

#endif

	return (0);
}

static int write_address (struct _diva_segment_alloc* pI, dword lo, dword hi, dword data) {
	diva_xdi_direct_access_cmd_t cmd;
	int ret;

	if (unlikely(pI->fd_xdi < 0))
		return (-1);


	cmd.hdr.ident = DIVA_XDI_DIRECT_ACCESS_CMD_IDENT;
	cmd.hdr.cmd   = DIVA_XDI_DIRECT_ACCESS_CMD_WRITE_BY_ADDRESS;
	cmd.cmd.write_by_address.address = lo;
	cmd.cmd.write_by_address.value   = data;

	ret = write (pI->fd_xdi, &cmd, sizeof(cmd.hdr)+sizeof(cmd.cmd.write_by_address)) ==
					sizeof(cmd.hdr)+sizeof(cmd.cmd.write_by_address) ? 0 : -1;

	return (ret);
}

static void resource_removed (struct _diva_segment_alloc* pI) {
#if defined(DIVA_USERMODE)

#else
	pI->d = &diva_segment_alloc_resource_removed_descriptor;
#endif
}

void  segment_free_proc(struct _diva_segment_alloc* pI, void* addr, dword lo, dword hi) {
	diva_entity_link_t* link;

	for (link = diva_q_get_head(&pI->busy_q); link != 0; link = diva_q_get_next(link)) {
		diva_map_entry_t* pE = DIVAS_CONTAINING_RECORD(link, diva_map_entry_t, link);

		if (pE->mem == addr && pE->dma_lo == lo && pE->dma_hi == hi) {
			diva_q_remove (&pI->busy_q, link);
			diva_q_add_tail (&pI->free_q, link);
			return;
		}
	}

	DBG_ERR(("segment not found: %p %08x:%08x [%p]", addr, lo, hi, pI))
}

dword get_segment_length_proc(struct _diva_segment_alloc* pI) {
	return (4*1024);
}

diva_segment_alloc_access_t* diva_get_segment_alloc_ifc (struct _diva_segment_alloc* segment_alloc) {
	return ((segment_alloc != 0) ? &segment_alloc->ifc : 0);
}


