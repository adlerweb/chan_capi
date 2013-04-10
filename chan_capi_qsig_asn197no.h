/*
 * An implementation of Common ISDN API 2.0 for Asterisk
 *
 * Copyright (C) 2005-2008 Cytronics & Melware
 * Copyright (C) 2007 Mario Goegel
 *
 * Armin Schindler <armin@melware.de>
 * Mario Goegel <m.goegel@gmx.de>
 *
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 */

/*
 *	Decoding of name-operations from asn1-97
 */

#ifndef PBX_QSIG_ASN197NO_H
#define PBX_QSIG_ASN197NO_H

#define ASN197NO_NAME_STRSIZE	50

extern unsigned int cc_qsig_asn197no_get_name(char *buf, int buflen, unsigned int *bufds, int *idx, unsigned char *data);

#endif
