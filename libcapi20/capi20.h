/*
 * CAPI 2.0 library
 *
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 *
 */
#ifndef __CAPI20_H__
#define __CAPI20_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <sys/time.h>

/* standard CAPI2.0 functions */

unsigned capi20_register (
	unsigned MaxLogicalConnection,
	unsigned MaxBDataBlocks,
	unsigned MaxBDataLen,
	unsigned *ApplIDp);

unsigned capi20_release (unsigned ApplID);

unsigned capi20_put_message (unsigned ApplID, unsigned char *Msg);

unsigned capi20_get_message (unsigned ApplID, unsigned char **Buf);

unsigned capi20_waitformessage(unsigned ApplID, struct timeval *TimeOut);

unsigned char *capi20_get_manufacturer (unsigned Ctrl, unsigned char *Buf);

unsigned char *capi20_get_version (unsigned Ctrl, unsigned char *Buf);

unsigned char *capi20_get_serial_number (unsigned Ctrl, unsigned char *Buf);

unsigned capi20_get_profile (unsigned Controller, unsigned char *Buf);

unsigned capi20_isinstalled (void);

int capi20_fileno(unsigned ApplID);

/* end standard CAPI2.0 functions */

/* extentions functions (no standard functions) */

int capi20ext_get_flags(unsigned ApplID, unsigned *flagsptr);
int capi20ext_set_flags(unsigned ApplID, unsigned flags);
int capi20ext_clr_flags(unsigned ApplID, unsigned flags);

char *capi20ext_get_tty_devname(
	unsigned applid,
	unsigned ncci,
	char *buf,
	size_t size);

char *capi20ext_get_raw_devname(
	unsigned applid,
	unsigned ncci,
	char *buf,
	size_t size);

int capi20ext_ncci_opencount(unsigned applid, unsigned ncci);

/* end extentions functions (no standard functions) */

#ifdef __cplusplus
}
#endif

#ifndef __NO_CAPIUTILS__
#include <capiutils.h>
#endif

#endif /* __CAPI20_H */

