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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
		
#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_utils.h"
#include "chan_capi_qsig.h"
#include "chan_capi_qsig_ecma.h"
#include "chan_capi_qsig_asn197ade.h"
#include "chan_capi_qsig_asn197no.h"

AST_MUTEX_DEFINE_STATIC(qsig_verbose_lock);

/*
 * helper for <pbx>_verbose with different verbose settings
 */
void cc_qsig_verbose(int c_d, char *text, ...)
{
	char line[4096];
	va_list ap;

	va_start(ap, text);
	vsnprintf(line, sizeof(line), text, ap);
	va_end(ap);

#if 0
	{
		FILE *fp;
		if ((fp = fopen("/tmp/cclog", "a")) != NULL) {
			fprintf(fp, "%s", line);
			fclose(fp);
		}
	}
#endif

	if (option_verbose > 4) {
		if ((!c_d) || ((c_d) && (capiqsigdebug))) {	
			cc_mutex_lock(&qsig_verbose_lock);
			cc_pbx_verbose("%s", line);
			cc_mutex_unlock(&qsig_verbose_lock);	
		}
	}
}


/*!
 * \brief Encodes an ASN.1 string with type of string
 *
 * \param asn1_type type of string like ASN1_OCTETSTRING
 * \param data pointer to target buffer
 * \param len size of target buffer
 * \param max_len of the string data
 * \param src source pointer for string
 * \param src_len string length
 * \return data length 
 */
int cc_qsig_asn1_add_string2(unsigned char asn1_type, void *data, int len, int max_len, void *src, int src_len)
{
	struct rose_component *comp = NULL;
	
	if (len < 2 + src_len)
		return -1;

	if (max_len && (src_len > max_len))
		src_len = max_len;

	comp = (struct rose_component *)data;
	comp->type = asn1_type;
	comp->len = src_len;
	memcpy(comp->data, src, src_len);
	
	return 2 + src_len;
}

/*!
 * \brief Encodes an ASN.1 string
 *
 * \param buf destination pointer for string
 * \param idx index points to string position in buffer
 * \param data string
 * \param datalen string length 
 * \return always zero 
 */
unsigned int cc_qsig_asn1_add_string(unsigned char *buf, int *idx, char *data, int datalen)
{
	int myidx=*idx;
	
	if ((1 + datalen + (*idx) ) > sizeof(*buf)) {
		/* String exceeds buffer size */
		return -1;
	}
	
	buf[myidx++] = datalen;
	memcpy(&buf[myidx], data, datalen);
	myidx += 1 + datalen;
	
	*idx = myidx;
	return 0;
}

/*
 * Returns an string from ASN.1 encoded string
 */
unsigned int cc_qsig_asn1_get_string(unsigned char *buf, int buflen, unsigned char *data)
{
	int strsize;
	int myidx=0;
	
	strsize = data[myidx++];
	if (strsize > buflen)
		strsize = buflen - 1;
	memcpy(buf, &data[myidx], strsize);
	buf[strsize] = 0;
	/* don't increase strsize after closing zero - string ends at strsize ! */
	
	return strsize;
}

/*
 * Encode ASN.1 Integer
 */
unsigned int cc_qsig_asn1_add_integer(unsigned char *buf, int *idx, int value)
{
	int myidx = *idx;
	int intlen = 1;
	
	if ((unsigned int)value > (unsigned int)0xFFFF)
		return -1;	/* no support at the moment */
	
	if (value > 255)
		intlen++;	/* we need 2 bytes */
	
	buf[myidx++] = ASN1_INTEGER;
	buf[myidx++] = intlen;
	if (intlen > 1)	{
		buf[myidx++] = (unsigned char)(value >> 8);
		buf[myidx++] = (unsigned char)(value - 0xff00);
	} else {
		buf[myidx++] = (unsigned char)value;
	}
	
	*idx = myidx;
	return 0;
}

/*
 * Returns an Integer from ASN.1 Encoded Integer
 */
unsigned int cc_qsig_asn1_get_integer(unsigned char *data, int *idx)
{	/* TODO: not conform with negative integers */
	int myidx = *idx;
	int intlen;
	int temp;
	
	intlen = data[myidx++];
	if ((intlen < 1) || (intlen > 2)) {  /* i don't know if there's a bigger Integer as 16bit -> read specs */
		cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "ASN1Decode: Size of ASN.1 Integer not supported: %i\n", intlen);
		*idx = myidx + intlen;
		return 0;
	}
	
	temp = (char)data[myidx++];
	if (intlen == 2) {
		temp=(temp << 8) + data[myidx++];
	}
	
	*idx = myidx;
	return temp;
}

/*
 * Returns an Human Readable OID from ASN.1 Encoded OID
 */
unsigned char *cc_qsig_asn1_oid2str(unsigned char *data, int size)
{
	unsigned char buf[1024];
	char numbuf[10];
	unsigned char *s;
	int len, i;
	unsigned long n;
	
	s = buf;
	if (size < 3) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "OID2STR: Object identifier too small (%i).\n", size);
		return NULL;
	}
	
#define N(n) \
		snprintf(numbuf, sizeof numbuf, "%lu", (unsigned long)n); \
		len = strlen(numbuf); \
		memcpy(s, numbuf, len); \
		s += len;
	
	N(data[0] / 40)
	*s++ = '.';
	N(data[0] % 40)
	n = 0;
	for (i = 1; i < size; i++) {
		n = n << 7 | (data[i] & 0x7f);
		if ((data[i] & 0x80) == 0) {
			*s++ = '.';
			N(n)
			n = 0;
		}
	}
	
	*s++ = 0;
	
	s = buf;
	return (unsigned char *) ast_strdup((char*)s);
	
}


/*
 * Check if OID is ECMA-ISDN (1.3.12.9.*)
 */
signed int cc_qsig_asn1_check_ecma_isdn_oid(unsigned char *data, int len)
{
	/*	1.3			.12		.9 */
	if ((data[0] == 0x2B) && (data[1] == 0x0C) && (data[2] == 0x09)) 
		return 0;
	return -1;
}


/*
 * This function simply updates the length informations of the facility struct
 */
void cc_qsig_update_facility_length(unsigned char * buf, unsigned int idx)
{
	buf[0] = idx;
	buf[2] = idx-2;
}

/*
 * Create Invoke Struct
 */
int cc_qsig_build_facility_struct(unsigned char * buf, unsigned int *idx, int protocolvar, int apdu_interpr, struct cc_qsig_nfe *nfe)
{
	int myidx = *idx;	/* we start with Index 1 - Byte 0 is Length of Facilitydataarray */
	if (!myidx)
		myidx++;
	
	buf[myidx++] = 0x1c;
	buf[myidx++] = 0;		/* Byte 2 length of Facilitydataarray */
	buf[myidx++] = 0x80 | protocolvar;	/* QSIG Facility */
	/* TODO: Outsource following struct to an separate function */
	buf[myidx++] = COMP_TYPE_NFE;		/* Network Facility Extension */
	buf[myidx++] = 6;				/* NFE Size hardcoded - not good */
	buf[myidx++] = 0x80;			/* Source Entity */
	buf[myidx++] = 0x01;
	buf[myidx++] = 0x00;			/* End PINX hardcoded */
	buf[myidx++] = 0x82;			/* Dest. Entity */
	buf[myidx++] = 0x01;
	buf[myidx++] = 0x00;			/* End PINX hardcoded */
	buf[myidx++] = COMP_TYPE_APDU_INTERP;	/* How to interpret this APDU */
	buf[myidx++] = 0x01;			/* Length */
	buf[myidx++] = apdu_interpr;
						/* Here will follow now the Invoke */
	*idx = myidx;
	cc_qsig_update_facility_length(buf, myidx);
	return 0;
}


/*
 * Add invoke to buf
 */
int cc_qsig_add_invoke(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i)
{
	unsigned char oid1[] = {0x2b,0x0c,0x09,0x00};
	int myidx = *idx;
	int invlenidx;
	int result;
	
	buf[myidx++] = COMP_TYPE_INVOKE;
	invlenidx = myidx;	/* save the Invoke length index for later */
	buf[myidx++] = 0;
	
	result = cc_qsig_asn1_add_integer(buf, &myidx, invoke->id);
	if (result) {
		cc_log(LOG_ERROR, "QSIG: Cannot add invoke, identifier is not encoded!\n");
		return -1;
	}
	
	if (invoke->descr_type == -1) {
		switch (i->qsigfeat) {
			case QSIG_TYPE_ALCATEL_ECMA:
				invoke->descr_type = ASN1_OBJECTIDENTIFIER;
				/* Set ECMA/ETSI OID */
				oid1[3] = (unsigned char)invoke->type;
				invoke->oid_len = sizeof(oid1);
				memcpy(invoke->oid_bin, oid1, sizeof(oid1));
				break;
			case QSIG_TYPE_HICOM_ECMAV2:
				invoke->descr_type = ASN1_INTEGER;
				/* Leave type as it is */
				break;
			default: 
				/* INVOKE is not encoded */
				break;
		}
	}
		
		
	switch (invoke->descr_type) {
		case ASN1_INTEGER:
			result = cc_qsig_asn1_add_integer(buf, &myidx, invoke->type);
			if (result) {
				cc_log(LOG_ERROR, "QSIG: Cannot add invoke, type is not encoded!\n");
				return -1;
			}
			break;
		case ASN1_OBJECTIDENTIFIER:
			if ((invoke->oid_len < 1) || (invoke->oid_len > 20)) {
				cc_log(LOG_ERROR, "QSIG: Cannot add invoke, OID is too big!\n");
				return -1;
			}
			buf[myidx++] = ASN1_OBJECTIDENTIFIER;
			buf[myidx++] = invoke->oid_len;
			memcpy(&buf[myidx], invoke->oid_bin, invoke->oid_len);
			myidx += invoke->oid_len;
			break;
		default:
			cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unknown Invoke Type, not encoded (%i)\n", invoke->descr_type);
			return -1;
			break;
	}
	if (invoke->datalen > 0) {	/* may be no error, if there's no data */
		memcpy(&buf[myidx], invoke->data, invoke->datalen);
		myidx += invoke->datalen;
	}
	
	buf[invlenidx] = myidx - invlenidx - 1;
	cc_qsig_update_facility_length(buf, myidx - 1);
	*idx = myidx;

	return 0;
}


		
/*
 * Valid QSIG-Facility?
 * Returns 0 if not
 */
unsigned int cc_qsig_check_facility(unsigned char *data, int *idx, int *apduval, int protocol)
{
	int myidx = *idx;
	char *APDU_STR[] = {"IGNORE", "REJECT CALL", "CLEAR CALL"};
	
	/* First byte after Facility Length */ 
	if (data[myidx] != (unsigned char)(0x80 | protocol)) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: received protocol 0x%#x not configured!\n", (data[myidx] ^= 0x80));
		return 0;
	}
	
	myidx++;
	cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Supplementary Services\n");
	if (data[myidx] == (unsigned char)COMP_TYPE_NFE) {
		myidx++;
		/* TODO: Check Entities? */
		myidx += data[myidx] + 1;
		*idx = myidx;
		cc_qsig_verbose( 1, VERBOSE_PREFIX_3  "QSIG: Facility has NFE struct\n");
	}
	if ((data[myidx] == (unsigned char)COMP_TYPE_APDU_INTERP)) {
		myidx++;
		myidx += data[myidx];
		*apduval = data[myidx++];
		/* TODO: implement real reject or clear call ? */
		*idx = myidx;
		 cc_qsig_verbose( 1, VERBOSE_PREFIX_3  "QSIG: Facility has APDU - What to do if INVOKE is unknown: %s\n", APDU_STR[*apduval]); 
	}
	return 1;
}


/*
 * Is this an INVOKE component?
 * when not return -1, set idx to next byte (length of component?)
 *		*** check idx in this case, that we are not out of range - maybe we got an unknown component then
 * when it is an invoke, return invoke length and set idx to first byte of component
 *
 */
signed int cc_qsig_check_invoke(unsigned char *data, int *idx)
{
	int myidx = *idx;
	
	if (data[myidx++] == (unsigned char)COMP_TYPE_INVOKE) {
		/* is an INVOKE */
		*idx = myidx;		/* Set index to length byte of component */
		return data[myidx + 1];	/* return component length */
	}
	*idx += data[myidx];	/* we can end here, if it is an Invoke Result or Error */
	return -1;			/* what to do now? got no Invoke */
}


/*
 * Get Invoke ID
 *	returns current index
 *	idx points to next byte in array
 */
signed int cc_qsig_get_invokeid(unsigned char *data, int *idx, struct cc_qsig_invokedata *invoke)
{
	int myidx;
	int invidtype = 0;
	int invlen = 0;
	int invoffset;
	int temp = 0;
	
	myidx = *idx;
	invoffset = myidx;
	invlen = data[myidx++];
	if (invlen > 0) {
		invoke->len = invlen;		/* set Length of Invoke struct */
		invoke->offset = invoffset;	/* offset in Facility Array, where the Invoke Data starts */
		invidtype = data[myidx++];	/* Get INVOKE Id Type */
		if (invidtype != ASN1_INTEGER) {
			cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unknown Invoke Identifier Type 0x%#x\n", invidtype);
			return -1;
		}
		temp = cc_qsig_asn1_get_integer(data, &myidx);
		invoke->id = temp;
 		*idx = myidx; 
	}
	return 0;
}


/*
 * fill the Invoke struct with all the invoke data
 */
signed int cc_qsig_fill_invokestruct(unsigned char *data, int *idx, struct cc_qsig_invokedata *invoke, int apduval)
{
	int myidx = *idx;
	int invoptyp;
	int temp;
	int temp2;
	int datalen;
	
	invoptyp = data[myidx++];		/* Invoke Operation Type 0x02=INTEGER, 0x06=OID */
	switch (invoptyp) {
		case ASN1_INTEGER:
			invoke->apdu_interpr = apduval;
			temp = cc_qsig_asn1_get_integer(data, &myidx);
			invoke->descr_type = ASN1_INTEGER;
			invoke->type = temp;
			temp2 = (invoke->len) + (invoke->offset) + 1;	/* Array End = Invoke Length + Invoke Offset +1 */
			datalen = temp2 - myidx;
					
			if (datalen > 255) {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unsupported INVOKE Operation Size (max 255 Bytes): %i\n", datalen);
				datalen = 255;
			}
			
			invoke->datalen = datalen;
			memcpy(invoke->data, &data[myidx], datalen);	/* copy data of Invoke Operation */
			myidx += datalen;		/* points to next INVOKE component, if there's any */
			*idx = myidx;
			
			break;
			
		case ASN1_OBJECTIDENTIFIER:
			invoke->apdu_interpr = apduval;
			invoke->descr_type = ASN1_OBJECTIDENTIFIER;
			temp = data[myidx++];		/* Length of OID */
			if (temp > 20)  {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unsupported INVOKE Operation OID Size (max 20 Bytes): %i\n", temp);
				temp = 20;
			}
			
			/* TODO: Maybe we decode the OID here and be verbose - have to write cc_qsig_asn1get_oid */
			
			invoke->oid_len = temp;
			memcpy(invoke->oid_bin, &data[myidx], temp);	/* Copy OID to separate array */
			myidx = myidx + temp;				/* Set index to next information */
			
			if (temp == 4) {	/* even if we have an OID, set the numeric invoke type */
				invoke->type = (int) invoke->oid_bin[3];
			} else {
				invoke->type = -1;
			}
			
			temp2 = (invoke->len) + (invoke->offset) + 1;	/* Array End = Invoke Length + Invoke Offset +1 */
			datalen = temp2 - myidx;
					
			if (datalen > 255) {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unsupported INVOKE Operation Size (max 255 Bytes): %i\n", datalen);
				datalen = 255;
			}
			
			invoke->datalen = datalen;
			memcpy(invoke->data, &data[myidx], datalen);	/* copy data of Invoke Operation */
			myidx += datalen;		/* points to next INVOKE component, if there's any */
			*idx = myidx;

			break;
			
		default:
			cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unknown INVOKE Operation Type: %i\n", invoptyp);
			
			temp2 = (invoke->len) + (invoke->offset) + 1;	/* Array End = Invoke Length + Invoke Offset +1 */
			datalen = temp2 - myidx;
					
			if (datalen > 255) {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unsupported INVOKE Operation Size (max 255 Bytes): %i\n", datalen);
				datalen = 255;
			}
			
			*idx = myidx + datalen;	/* Set index to next INVOKE, if there's any */
			return -1;
			break;
	}
	return 0;	/* No problems */
	
}

static int ident_qsig_invoke(int invoketype)
{
	switch (invoketype) {
		case 0:
		case 1:
		case 2:
		case 3:
			return CCQSIG__ECMA__NAMEPRES;
		case 4:
			return CCQSIG__ECMA__PRPROPOSE;
		case 12:
			return CCQSIG__ECMA__CTCOMPLETE;
		case 21:
			return CCQSIG__ECMA__LEGINFO2;
		default:
			cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "QSIG: Unhandled QSIG INVOKE (%i)\n", invoketype);
			return -1;
	}
}

/*
 * Identify an INVOKE and return our own Ident Integer (CCQSIG__*)
 */
signed int cc_qsig_identifyinvoke(struct cc_qsig_invokedata *invoke, int protocol)
{
	int invokedescrtype = 0;
	int datalen;
	
/*	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "CONNECT_IND (Ident Invoke %i)\n", invoke->descr_type); */

	switch (protocol) {
		case QSIG_TYPE_ALCATEL_ECMA:
			switch (invoke->descr_type) {
				case ASN1_INTEGER:
					invokedescrtype = 1;
					cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: INVOKE OP (%i)\n", invoke->type);
					return ident_qsig_invoke(invoke->type);
					break;
				case ASN1_OBJECTIDENTIFIER:
					invokedescrtype = 2;
					datalen = invoke->oid_len;
					
					{
						unsigned char *oidstr = NULL;
						oidstr = cc_qsig_asn1_oid2str(invoke->oid_bin, invoke->oid_len);
						if (oidstr) {
							cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: INVOKE OP (%s)\n", oidstr);
							ast_free(oidstr);
						} else {
							cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: INVOKE OP (unknown - OID not displayable)\n");
						}
					}
					
					if ((datalen) == 4) {
						if (!cc_qsig_asn1_check_ecma_isdn_oid(invoke->oid_bin, datalen)) {
							return ident_qsig_invoke( (int)invoke->oid_bin[3]);
						}
					}
					return -1;
					break;
				default:
					cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unidentified INVOKE OP\n");
					break;
			}
			break;
		case QSIG_TYPE_HICOM_ECMAV2:
			switch (invoke->descr_type) {
				case ASN1_INTEGER:
					invokedescrtype = 1;
					cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: INVOKE OP (%i)\n", invoke->type);
					return ident_qsig_invoke(invoke->type);
					break;
				case ASN1_OBJECTIDENTIFIER:
					invokedescrtype = 2;
					datalen = invoke->oid_len;
					
					{
						unsigned char *oidstr = NULL;
						oidstr = cc_qsig_asn1_oid2str(invoke->oid_bin, invoke->oid_len);
						if (oidstr) {
							cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: INVOKE OP (%s)\n", oidstr);
							ast_free(oidstr);
						} else {
							cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: INVOKE OP (unknown - OID not displayable)\n");
						}
					}
					
					if ((datalen) == 4) {
						if (!cc_qsig_asn1_check_ecma_isdn_oid(invoke->oid_bin, datalen)) {
							return ident_qsig_invoke( (int)invoke->oid_bin[3]);
						}
					}
					break;
				default:
					cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Unidentified INVOKE OP\n");
					break;
			}
			break;
		default:
			break;
	}
	return -1;
	
}

/*
 * find the interface (pvt) the PLCI belongs to
 */
static struct capi_pvt *capi_find_interface_bynumber(char * num)
{
	struct capi_pvt *i;

	if (!num)
		return NULL;

	for (i = capi_iflist; i; i = i->next) {
		if (strcmp(i->cid, num) == 0 ||
		    strcmp(i->dnid, num) == 0)
			return i;
	}

#if 0
	cc_mutex_lock(&nullif_lock);
	for (i = nulliflist; i; i = i->next) {
		if (strcmp(i->cid, num) == 0 ||
		    strcmp(i->dnid, num) == 0)
			break;
	}
	cc_mutex_unlock(&nullif_lock);
#endif
	
	return i;
}

static void pbx_capi_qsig_handle_ctc(struct cc_qsig_invokedata *invoke, struct capi_pvt *i)
{
	struct cc_qsig_ctcomplete ctc;
	struct capi_pvt *ii;
	
#define CLEAR_CTC {	if (ctc.redirectionNumber.partyNumber)	free_null(ctc.redirectionNumber.partyNumber);\
			if (ctc.basicCallInfoElements)	free_null(ctc.basicCallInfoElements); \
			if (ctc.redirectionName) free_null(ctc.redirectionName); \
			if (ctc.argumentExtension) free_null(ctc.argumentExtension); }

	int res = cc_qsig_decode_ecma_calltransfer(invoke, i, &ctc);
	
	if (!res)
		return;
	
	if (ctc.redirectionNumber.partyNumber && (ctc.endDesignation == 0)) { 
		ii = capi_find_interface_bynumber(ctc.redirectionNumber.partyNumber);
		if (ii) {
			char *prpn = i->qsig_data.if_pr_propose_pn;
			cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Call Transfer partner channel for %s found at channel %s, bridging possible.\n", ctc.redirectionNumber.partyNumber, ii->vname);
			
			if (!(strlen(prpn))) {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Path Replacement not configured, bridging not available!\n");
			} else {
				unsigned char fac[CAPI_MAX_FACILITYDATAARRAY_SIZE];
					
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: Trying to bridge with Path Replacement number %s...\n", prpn);
				
				switch (ii->state) {
					case CAPI_STATE_ALERTING:
						cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: peer is in state ALERTING, PATH REPLACE follows after CONNECT...\n");
 						ii->qsig_data.pr_propose_cid = ast_strdup("123");	/* HACK: need an dynamic ID */
 						ii->qsig_data.pr_propose_pn = ast_strdup(i->qsig_data.if_pr_propose_pn);
 						ii->qsig_data.pr_propose_doinboundbridge = 1;
						i->qsig_data.pr_propose_doinboundbridge = 1;
						i->qsig_data.partner_plci = ii->PLCI;
						break;
					case CAPI_STATE_CONNECTED:
						cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: peer is CONNECTED...\n");
 						i->qsig_data.pr_propose_cid = ast_strdup("123");	/* HACK: need an dynamic ID */
 						i->qsig_data.pr_propose_pn = ast_strdup(i->qsig_data.if_pr_propose_pn);
						ii->qsig_data.pr_propose_doinboundbridge = 1;
  						ii->qsig_data.partner_plci = i->PLCI;
						
 						cc_qsig_do_facility(fac, i->owner, NULL, 4, 0);
							
 						capi_sendf(NULL, 0, CAPI_INFO_REQ, i->PLCI, get_capi_MessageNumber(),
 								"()(()()()s)", fac  );
						
						i->qsig_data.pr_propose_cid = NULL;
						i->qsig_data.pr_propose_pn = NULL;
						break;
					default:
						cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "QSIG: peer's state is %i, which is not handled yet...\n", ii->state);
						break;
				} 
				
			} 
			
  			CLEAR_CTC
			
		}
	}
	
  	CLEAR_CTC
		
	return ;
}

/*
 * Handle inbound INVOKEs
 */
unsigned int cc_qsig_handle_invokeoperation(int invokeident, struct cc_qsig_invokedata *invoke, struct capi_pvt *i)
{
	switch (invokeident) {
		case CCQSIG__ECMA__NAMEPRES:
			cc_qsig_op_ecma_isdn_namepres(invoke, i);
			break;
		case CCQSIG__ECMA__PRPROPOSE:
			cc_qsig_op_ecma_isdn_prpropose(invoke, i);
			break;
		case CCQSIG__ECMA__CTCOMPLETE:
			pbx_capi_qsig_handle_ctc(invoke, i);
			break;
		case CCQSIG__ECMA__LEGINFO2:
			cc_qsig_op_ecma_isdn_leginfo2(invoke, i);
			break;
		default:
			break;
	}
	return 0;
}

/**
 * Handles incoming facilities
 *
 * @internal
 * @param data facility buffer
 * @param i points to capip_pvt struct
 * @param idx index in facility buffer
 * @param faclen facility length
 * @param protocoltype Q932_PROTOCOL_ROSE | Q932_PROTOCOL_EXTENSIONS
 * @return zero
 */
static int qsig_handle_q932facility(unsigned char *data, struct capi_pvt *i, int *idx, int faclen, int protocoltype)
{
	int facidx = *idx;
	int action_unkn_apdu;		/* What to do with unknown Invoke-APDUs (0=Ignore, 1=clear call, 2=reject APDU) */
	struct cc_qsig_invokedata invoke;
	int invoke_len;
	int invoketmp1;
	unsigned int invoke_op = 0;	/* Invoke Operation ID */
		
	if (cc_qsig_check_facility(data, &facidx, &action_unkn_apdu, protocoltype)) {
		while ((facidx) < faclen) {
			cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Checking INVOKE at index %i (of %i)\n", facidx, faclen);
			invoke_len = cc_qsig_check_invoke(data, &facidx);
			if (invoke_len > 0) {
				if (cc_qsig_get_invokeid(data, &facidx, &invoke) == 0) {
					invoketmp1 = cc_qsig_fill_invokestruct(data, &facidx, &invoke, action_unkn_apdu);
					invoke_op = cc_qsig_identifyinvoke(&invoke, i->qsigfeat);
					if (invoke_op < 0) {
						cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Invoke not identified!\n");
					}
					cc_qsig_handle_invokeoperation(invoke_op, &invoke, i);
				}
			} else {
				/* Not an Invoke */
			}
		}
	} else { /* kill endlessloop */
		facidx += faclen;
	}
	*idx = facidx;
	return 0;
}

/*
 * Handles incoming Indications from CAPI
 */
unsigned int cc_qsig_handle_capiind(unsigned char *data, struct capi_pvt *i)
{
	int faclen0 = 0;
	int faclen = 0;
	int facidx = 2;
	
	if (!i->qsigfeat)
		return 0;
	
	if (!data) {
		return 0;
	}

	faclen0 = data[facidx-2];	/* Length of facility array - there may be more facilities encoded in this struct */
	faclen = data[facidx++];
 	faclen += facidx;
	while (facidx < faclen0) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Checking Facility at index %i\n", facidx);
		switch (i->qsigfeat) {
			case QSIG_TYPE_ALCATEL_ECMA:
				qsig_handle_q932facility(data, i, &facidx, faclen, Q932_PROTOCOL_ROSE);
				break;
			case QSIG_TYPE_HICOM_ECMAV2:
				qsig_handle_q932facility(data, i, &facidx, faclen, Q932_PROTOCOL_EXTENSIONS);
				break;
			default:
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Unknown QSIG protocol configured (%i)\n", i->qsigfeat);
				break;
		}
		
		if (facidx < faclen0) {	/* there may follow a new facility */
			if (data[facidx] == 0x1c) {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Found another facility at index %i\n", facidx);
				facidx++;
				faclen = data[facidx++];
				faclen += facidx;
			} else {
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "More data found in facility at index %i, but this is not an facility (%#x)\n", facidx, data[facidx]);
				facidx++; /* don't start an endlessloop */
			}
		}
	}
	cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Facility done at index %i from %i\n", facidx, faclen);
	return 1;
}

/*
 * Handles incoming Facility Indications from CAPI
 */
unsigned int cc_qsig_handle_capi_facilityind(unsigned char *data, struct capi_pvt *i)
{
	int faclen = 0;
	int facidx = 0;

	if (!data) {
		return 0;
	}
	faclen = data[facidx++];
	/*					cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "CONNECT_IND (Got Facility IE, Length=%#x)\n", faclen); */
	while (facidx < faclen) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Checking Facility at index %i\n", facidx);
		switch (i->qsigfeat) {
			case QSIG_TYPE_ALCATEL_ECMA:
				qsig_handle_q932facility(data, i, &facidx, faclen, Q932_PROTOCOL_ROSE);
				break;
			case QSIG_TYPE_HICOM_ECMAV2:
				qsig_handle_q932facility(data, i, &facidx, faclen, Q932_PROTOCOL_EXTENSIONS);
				break;
			default:
				cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Unknown QSIG protocol configured (%i)\n", i->qsigfeat);
				/* kill endlessloop */
				facidx += faclen;
				break;
		}
	}
	cc_qsig_verbose( 1, VERBOSE_PREFIX_3 "Facility done at index %i from %i\n", facidx, faclen);
	return 1;
}

static int identify_qsig_setup_callfeature(char *param)
{
	char *p = param;
	switch (*p) {
		case 't':
			cc_qsig_verbose( 1, "Call Transfer");
			p++;
			if (*p == 'r') {
				cc_qsig_verbose( 1, " on ALERT");
				return 2;
			} else {
				return 1;
			}
		default:
			cc_qsig_verbose( 1, "unknown (%c)\n", *p);
			break;
	}
	
	return 0;
}

/*
 * Handles outgoing Facilies on Call SETUP
 */
unsigned int cc_qsig_add_call_setup_data(unsigned char *data, struct capi_pvt *i, struct  ast_channel *c)
{
	/* TODO: Check buffers */
	struct cc_qsig_invokedata invoke;
	struct cc_qsig_nfe nfe;
	unsigned int dataidx = 0;
	int protocolvar = 0;
	
	const unsigned char xprogress[] = {0x1e,0x02,0xa0,0x90};
	char *p = NULL;
	char *pp = NULL;
	int add_externalinfo = 0;
			
	data[0] = 0; /* Initialize array length */
	
	if ((p = (char *)pbx_builtin_getvar_helper(c, "QSIG_SETUP"))) {
		/* some special dial parameters */
		/* parse the parameters */
		while ((p) && (*p)) {
			switch (*p) {
				case 'X':	/* add PROGRESS INDICATOR for external calls*/
					cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "Sending QSIG external PROGRESS IE.\n");
					add_externalinfo = 1;
 					pp = strsep (&p, "/");
 					pp = NULL;
					break;
				case 'C':
					cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "QSIG Call Feature requested: ");
					p++;
					switch(identify_qsig_setup_callfeature(p)) {
						case 1: /* Call transfer */
							p++;
 							pp = strsep(&p, "/");
							if (!pp) {
								cc_log(LOG_WARNING, "QSIG Call Feature needs plci as parameter!\n");
							} else {
								i->qsig_data.calltransfer = 1;
								i->qsig_data.partner_plci = atoi(pp);
								/* set the other channel as partner to me */
								{
									struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);
									if (ii)
										ii->qsig_data.partner_plci = i->PLCI;
								}
								
								cc_qsig_verbose( 1, " for plci %#x\n", i->qsig_data.partner_plci);
							}
							break;
						case 2: /* Call transfer on ring */
							p += 2;
 							pp = strsep(&p, "/");
							if (!pp) {
								cc_log(LOG_WARNING, "QSIG Call Feature needs plci as parameter!\n");
							} else {
								i->qsig_data.calltransfer_onring = 1;
								i->qsig_data.partner_plci = atoi(pp);
								/* set the other channel as partner to me */
								{
									struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);
									if (ii)
										ii->qsig_data.partner_plci = i->PLCI;
								}
								
								cc_qsig_verbose( 1, " for plci %#x\n", i->qsig_data.partner_plci);
							}
							break;
						default:
 							pp = strsep(&p, "/");
							break;
					}
					pp = NULL;
					break;
				default:
					cc_log(LOG_WARNING, "Unknown parameter '%c' in QSIG_SETUP, ignoring.\n", *p);
					p++;
			}
		}
	}
	
	switch (i->qsigfeat) {
		case QSIG_TYPE_ALCATEL_ECMA:
			protocolvar = Q932_PROTOCOL_ROSE;
			break;
		case QSIG_TYPE_HICOM_ECMAV2:
			protocolvar = Q932_PROTOCOL_EXTENSIONS;
			break;
		default:
			cc_log(LOG_WARNING, " Unknown QSIG variant configured.\n");
			return 0;
			break;
	}
		
	cc_qsig_build_facility_struct(data, &dataidx, protocolvar, APDUINTERPRETATION_IGNORE, &nfe);
	cc_qsig_encode_ecma_name_invoke(data, &dataidx, &invoke, i, 0, pbx_capi_get_connectedname (i->owner, ""));
	cc_qsig_add_invoke(data, &dataidx, &invoke, i);
	
	if (add_externalinfo) {
		/* add PROGRESS INDICATOR for external calls*/
		int progress_size = sizeof(xprogress);
		memcpy(&data[dataidx], xprogress, progress_size);
		data[0] += progress_size;
		dataidx += progress_size;
	}
	
	return 0;
}

/*
 * Handles outgoing Facilies on Call Answer
 */
unsigned int cc_qsig_add_call_answer_data(unsigned char *data, struct capi_pvt *i, struct  ast_channel *c)
{
	struct cc_qsig_invokedata invoke;
	struct cc_qsig_nfe nfe;
	unsigned int dataidx = 0;
	int protocolvar = 0;
	const char *connectedname;
	
	data[0] = 0;

	if (!i->qsigfeat)
		return 0;
	
	if (!(connectedname = pbx_builtin_getvar_helper(c, "CONNECTEDNAME")))
		return 0;
	
	if (!strlen(connectedname)) 
		return 0;
	
	switch (i->qsigfeat) {
		case QSIG_TYPE_ALCATEL_ECMA:
			protocolvar = Q932_PROTOCOL_ROSE;
			break;
		case QSIG_TYPE_HICOM_ECMAV2:
			protocolvar = Q932_PROTOCOL_EXTENSIONS;
			break;
		default:
			cc_log(LOG_WARNING, " Unknown QSIG variant configured.\n");
			return 0;
			break;
	}

	cc_qsig_build_facility_struct(data, &dataidx, protocolvar, APDUINTERPRETATION_IGNORE, &nfe);
	cc_qsig_encode_ecma_name_invoke(data, &dataidx, &invoke, i, 2, (char *)connectedname);
	cc_qsig_add_invoke(data, &dataidx, &invoke, i);

	return 1;
}

/*
 * Handles outgoing Facilies on Call Alert
 */
unsigned int cc_qsig_add_call_alert_data(unsigned char *data, struct capi_pvt *i, struct  ast_channel *c)
{
	struct cc_qsig_invokedata invoke;
	struct cc_qsig_nfe nfe;
	unsigned int dataidx = 0;
	int protocolvar = 0;
	const char *connectedname;
	
	data[0] = 0;
	
	if (!i->qsigfeat)
		return 0;
	
 	if (!(connectedname = pbx_builtin_getvar_helper(c, "CALLEDNAME")))
		return 0;
		
	if (!strlen(connectedname)) 
		return 0;
	
	switch (i->qsigfeat) {
		case QSIG_TYPE_ALCATEL_ECMA:
			protocolvar = Q932_PROTOCOL_ROSE;
			break;
		case QSIG_TYPE_HICOM_ECMAV2:
			protocolvar = Q932_PROTOCOL_EXTENSIONS;
			break;
		default:
			cc_log(LOG_WARNING, " Unknown QSIG variant configured.\n");
			return 0;
			break;
	}

	cc_qsig_build_facility_struct(data, &dataidx, protocolvar, APDUINTERPRETATION_IGNORE, &nfe);
	cc_qsig_encode_ecma_name_invoke(data, &dataidx, &invoke, i, 1, (char *)connectedname);
	cc_qsig_add_invoke(data, &dataidx, &invoke, i);

	return 1;
}

/*
 * Handles outgoing Facilies on capicommand
 */
unsigned int cc_qsig_do_facility(unsigned char *fac, struct  ast_channel *c, char *param, unsigned int factype, int info1)
{
	struct cc_qsig_invokedata invoke;
	struct cc_qsig_nfe nfe;
	struct capi_pvt *i = CC_CHANNEL_PVT(c);
	/* struct capi_pvt *ii = NULL; */
	unsigned int facidx = 0;
	int protocolvar = 0;
	
	switch (i->qsigfeat) {
		case QSIG_TYPE_ALCATEL_ECMA:
			protocolvar = Q932_PROTOCOL_ROSE;
			break;
		case QSIG_TYPE_HICOM_ECMAV2:
			protocolvar = Q932_PROTOCOL_EXTENSIONS;
			break;
		default:
			cc_log(LOG_WARNING, " Unknown QSIG variant configured.\n");
			return 0;
			break;
	}
	
	cc_qsig_build_facility_struct(fac, &facidx, protocolvar, APDUINTERPRETATION_IGNORE, &nfe);
	switch (factype) {
		case 4: /* ECMA-xxx pathReplacementPropose */
			cc_qsig_encode_ecma_prpropose(fac, &facidx, &invoke, i, param);
			cc_qsig_add_invoke(fac, &facidx, &invoke, i);
			
			break;
		case 12: /* ECMA-178 callTransfer */
			cc_qsig_encode_ecma_calltransfer(fac, &facidx, &invoke, i, param, info1);
			cc_qsig_add_invoke(fac, &facidx, &invoke, i);
			
			break;
		case 99: /* ECMA-300 simpleCallTransfer */
			cc_qsig_encode_ecma_sscalltransfer(fac, &facidx, &invoke, i, param);
			cc_qsig_add_invoke(fac, &facidx, &invoke, i);
			break;
		default:
			break;
	}
	
	return 0;
}

/*
 * capicommand getplci - needed for Call Transfer
 */
int pbx_capi_qsig_getplci(struct ast_channel *c, char *param)
{
	struct capi_pvt *i = CC_CHANNEL_PVT(c);
	char buffer[10];

	snprintf(buffer, sizeof(buffer)-1, "%d", i->PLCI);
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "QSIG_GETPLCI: %s\n", buffer);
	pbx_builtin_setvar_helper(c, "QSIG_PLCI", buffer);
	
	return 0;
}

/*
 * Initiate a QSIG Single Step Call Transfer
 */
int pbx_capi_qsig_ssct(struct ast_channel *c, char *param)
{
	unsigned char fac[CAPI_MAX_FACILITYDATAARRAY_SIZE];
	struct capi_pvt *i = CC_CHANNEL_PVT(c);

	if (!param) { /* no data implies no Calling Number and Destination Number */
		cc_log(LOG_WARNING, CC_MESSAGE_NAME " qsig_ssct requires source number and destination number\n");
		return -1;
	}

	cc_qsig_do_facility(fac, c, param, 99, 0);
	
	capi_sendf(NULL, 0, CAPI_INFO_REQ, i->PLCI, get_capi_MessageNumber(),
		"()(()()()s)",
		fac
	);

	return 0;
}

/*
 * Initiate a QSIG Call Transfer
 */
int pbx_capi_qsig_ct(struct ast_channel *c, char *param)
{
	unsigned char fac[CAPI_MAX_FACILITYDATAARRAY_SIZE];
	struct capi_pvt *i = CC_CHANNEL_PVT(c);
	struct capi_pvt *ii = NULL;
	unsigned int callmark;
	char *marker;

	if (!param) { /* no data implies no Calling Number and Destination Number */
		cc_log(LOG_WARNING, CC_MESSAGE_NAME " qsig_ct requires call marker, source number, destination number and await_connect info\n");
		return -1;
	}

	marker = strsep(&param, COMMANDSEPARATOR);
	
	callmark = atoi(marker);
	cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * QSIG_CT: using call marker %i(%s)\n", callmark, marker);
	
	for (ii = capi_iflist; ii; ii = ii->next) {
		if (ii->qsig_data.callmark == callmark)
			break;
	}
	
	if (!ii) {
		cc_log(LOG_WARNING, CC_MESSAGE_NAME " qsig_ct call marker not found!\n");
		return -1;
	}
	
	cc_qsig_do_facility(fac, c, param, 12, 1);
	
	capi_sendf(NULL, 0, CAPI_INFO_REQ, i->PLCI, get_capi_MessageNumber(),
		"()(()()()s())",
		fac
	);

	cc_qsig_do_facility(fac, c, param, 12, 0);
	
	capi_sendf(NULL, 0, CAPI_INFO_REQ, ii->PLCI, get_capi_MessageNumber(),
		   "()(()()()s())",
		   fac
	);
	

	return 0;
}

/*
 * mark an call
 */
int pbx_capi_qsig_callmark(struct ast_channel *c, char *param)
{
	struct capi_pvt *i = CC_CHANNEL_PVT(c);

	if (!param) { /* no data implies no Calling Number and Destination Number */
		cc_log(LOG_WARNING, "capi qsig_callmark requires an call identifier\n");
		return -1;
	}

	i->qsig_data.callmark = atoi(param);

	return 0;
}


static void send_feature_calltransfer(struct capi_pvt *i)
{
	unsigned char *fac = alloca(CAPI_MAX_FACILITYDATAARRAY_SIZE);

	struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);

	/* needed for Path Replacement */
	ii->qsig_data.partner_plci = i->PLCI;

	if (ii) {
		cc_qsig_do_facility(fac, i->owner, NULL, 12, 1);
		
		capi_sendf(NULL, 0, CAPI_INFO_REQ, i->PLCI, get_capi_MessageNumber(), "()(()()()s())", fac);
		
		cc_qsig_do_facility(fac, ii->owner, NULL, 12, 0);

		capi_sendf(NULL, 0, CAPI_INFO_REQ, ii->PLCI, get_capi_MessageNumber(), "()(()()()s())", fac);
	} else {
		cc_log(LOG_WARNING, "Call Transfer failed - second channel not found (PLCI %#x)!\n", i->qsig_data.partner_plci);
	}

}

/*
 * init QSIG data on new channel - will be called by mkif
 */
void cc_qsig_interface_init(struct cc_capi_conf *conf, struct capi_pvt *tmp)
{
	tmp->qsigfeat = conf->qsigfeat;
	if (!conf->qsigfeat) 
		return;
		
	tmp->qsig_data.calltransfer_active = 0;
	tmp->qsig_data.calltransfer = 0;
	tmp->qsig_data.calltransfer_onring = 0;
	tmp->qsig_data.callmark = 0;
	tmp->qsig_data.dnameid = NULL;

	/* Path Replacement */
	tmp->qsig_data.pr_propose_active = 0;
	tmp->qsig_data.pr_propose_sendback = 0; /* send back an prior received PR PROPOSE on Connect */
	tmp->qsig_data.pr_propose_sentback = 0;
	tmp->qsig_data.pr_propose_doinboundbridge = 0;
	tmp->qsig_data.pr_propose_cid = NULL;	/* Call identity */
	tmp->qsig_data.pr_propose_pn = NULL;	/* Party Number */
	
	cc_copy_string(tmp->qsig_data.if_pr_propose_pn, conf->qsigconf.if_pr_propose_pn, sizeof(tmp->qsig_data.if_pr_propose_pn));
	
	/* Partner Channel - needed for many features */
	tmp->qsig_data.partner_ch = NULL;
	tmp->qsig_data.partner_plci = 0;
	tmp->qsig_data.waitevent = 0;
	
 	ast_cond_init(&tmp->qsig_data.event_trigger, NULL);
}

/*
 * build the qsig-interface according to configs
 */
void cc_pbx_qsig_conf_interface_value(struct cc_capi_conf *conf, struct ast_variable *v)
{
#define CONF_STRING(var, token)            \
	if (!strcasecmp(v->name, token)) { \
	cc_copy_string(var, v->value, sizeof(var)); \
	}
#define CONF_INTEGER(var, token)           \
	if (!strcasecmp(v->name, token)) { \
	var = atoi(v->value);      \
}
#define CONF_TRUE(var, token, val)         \
	if (!strcasecmp(v->name, token)) { \
	if (ast_true(v->value))    \
	var = val;         \
}

	CONF_INTEGER(conf->qsigfeat, "qsig")
	CONF_TRUE(conf->divaqsig, "divaqsig", 1)
	CONF_STRING(conf->qsigconf.if_pr_propose_pn, "qsig_prnum")

	
#undef CONF_STRING
#undef CONF_INTEGER
#undef CONF_TRUE
}

/*
 * cleanup QSIG stuff on every (end of) call per interface
 */
static void qsig_cleanup_channel(struct  capi_pvt *i)
{
	i->qsig_data.callmark = 0;
	i->qsig_data.partner_ch = NULL;
	i->qsig_data.calltransfer_active = 0;
	i->qsig_data.calltransfer_onring = 0;
	i->qsig_data.pr_propose_active = 0;
	i->qsig_data.pr_propose_sentback = 0;
	i->qsig_data.pr_propose_doinboundbridge = 0;
	if (i->qsig_data.pr_propose_cid) {
		ast_free(i->qsig_data.pr_propose_cid);
		i->qsig_data.pr_propose_cid = NULL;
	}
	if (i->qsig_data.pr_propose_pn) {
		ast_free(i->qsig_data.pr_propose_pn);
		i->qsig_data.pr_propose_pn = NULL;
	}
	if (i->qsig_data.dnameid) {
		ast_free(i->qsig_data.dnameid);
		i->qsig_data.dnameid = NULL;
	}
	
}

/*
 * cleanup QSIG stuff on interface
 */
void interface_cleanup_qsig(struct capi_pvt *i)
{
	if (i->qsigfeat) {
		cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "QSIG: cleanup channel\n");
		qsig_cleanup_channel(i);
	}
}

/*
 * cleanup QSIG stuff on module unload
 */
void pbx_capi_qsig_unload_module(struct capi_pvt *i)
{
	if (!i->qsigfeat)
		return;
	ast_cond_destroy(&i->qsig_data.event_trigger);
}

/*
 * wait for B3 up
 */
int pbx_capi_qsig_wait_for_prpropose(struct capi_pvt *i)
{
	struct timespec abstime;
	int ret = 1;

  	cc_mutex_lock(&i->lock);
	if (!(i->qsig_data.pr_propose_sentback)) {
		i->qsig_data.waitevent = CAPI_QSIG_WAITEVENT_PRPROPOSE;
		abstime.tv_sec = time(NULL) + CCQSIG_TIMER_WAIT_PRPROPOSE;  /* PR PROPOSE TIMER */
		abstime.tv_nsec = 0;
		cc_qsig_verbose( 1, "%s: wait for PATH REPLACEMENT.\n",
			   i->vname);
		if (ast_cond_timedwait(&i->qsig_data.event_trigger, &i->lock, &abstime) != 0) {
			cc_log(LOG_WARNING, "%s: timed out waiting for PATH REPLACEMENT.\n",
			       i->vname);
			ret = 0;
		} else {
			cc_qsig_verbose( 1, "%s: cond signal received for PATH REPLACEMENT.\n",
				   i->vname);
		}
	}
	cc_mutex_unlock(&i->lock);

	return ret;
}

/*
 * check special conditions, wake waiting threads and send outstanding commands
 * for the given interface
 */
static void pbx_capi_qsig_post_handling(struct capi_pvt *i)
{
	if ((i->qsig_data.waitevent == CAPI_QSIG_WAITEVENT_PRPROPOSE) &&
		    (i->qsig_data.pr_propose_sentback == 1)) {
		i->qsig_data.waitevent = 0;
 		ast_cond_signal(&i->qsig_data.event_trigger);
		cc_qsig_verbose( 1, "%s: found and signal for PATH REPLACEMENT state.\n",
			   i->vname);
		return;
	}
}

/* 
 * handle a bridge attempt - maybe we're allowed to make a path replacement
 */
int pbx_capi_qsig_bridge(struct capi_pvt *i0,struct capi_pvt *i1)
{
	if (i1->qsig_data.pr_propose_sentback) {
		return 2; /* Path Replacement already sent out - call will be cleared in short */
	}
	
	i1->qsig_data.partner_plci = i0->PLCI;
	send_feature_calltransfer(i1);	
	
	if (pbx_capi_qsig_wait_for_prpropose(i1))
		return 1; /* Path Replacement successful */

	/* No Path Replacement - allow line interconnect */
	return 0;

}

int pbx_capi_qsig_sendtext(struct ast_channel *c, const char *text)
{
	struct capi_pvt *i = CC_CHANNEL_PVT(c);
	
#if 0
	/* suppress compiler warnings */
	unsigned char *data = alloca(CAPI_MAX_FACILITYDATAARRAY_SIZE);
	unsigned int dataidx = 0;
	struct cc_qsig_invokedata invoke;
	struct cc_qsig_nfe nfe;
#endif
	
	int protocolvar = 0;
	
	if (!i->qsigfeat)
		return 0;
	
	
	if (!strlen(text)) 
		return 0;
	
	switch (i->qsigfeat) {
		case QSIG_TYPE_ALCATEL_ECMA:
			protocolvar = Q932_PROTOCOL_ROSE;
			break;
		case QSIG_TYPE_HICOM_ECMAV2:
			protocolvar = Q932_PROTOCOL_EXTENSIONS;
			break;
		default:
			cc_log(LOG_WARNING, " Unknown QSIG variant configured.\n");
			return 0;
			break;
	}

	/* TODO: implement something - QSIG_LEG_INFO3 doesn't work here */	
	
	return 0;
}
		
/*
 *  CAPI INFO_IND (QSIG part)
 */
void pbx_capi_qsig_handle_info_indication(_cmsg *CMSG, unsigned int PLCI, unsigned int NCCI, struct capi_pvt *i)
{
	if (!i->qsigfeat)	/* Run only, if QSIG enabled */
		return;
	
	switch(INFO_IND_INFONUMBER(CMSG)) {
		case 0x0008:	/* Cause */
			break;
		case 0x0014:	/* Call State */
			break;
		case 0x0018:	/* Channel Identification */
			break;
		case 0x001c:	/*  Facility Q.932 */
			{
				unsigned int qsiginvoke;
				qsiginvoke = cc_qsig_handle_capi_facilityind( (unsigned char*) INFO_IND_INFOELEMENT(CMSG), i);
			
				/* got an Path Replacement */
				if ((i->qsig_data.pr_propose_cid && i->qsig_data.pr_propose_pn) &! i->qsig_data.pr_propose_sendback &! i->qsig_data.pr_propose_doinboundbridge) {
					struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);
					
					if (ii) {
						if (ii->state == CAPI_STATE_CONNECTED) { /* second line is connected, we can proceed */
							unsigned char fac[CAPI_MAX_FACILITYDATAARRAY_SIZE];
					
							cc_qsig_do_facility(fac, i->owner, NULL, 4, 0);
							
							capi_sendf(NULL, 0, CAPI_INFO_REQ, ii->PLCI, get_capi_MessageNumber(),
									"()(()()()s)",
									fac
							);
							i->qsig_data.pr_propose_active = 1;
							ii->qsig_data.pr_propose_sentback = 1;
						} else { /* Path Replacement has to be sent back after Connect on second line */
							ii->qsig_data.pr_propose_sendback = 1;
							ii->qsig_data.pr_propose_cid = ast_strdup(i->qsig_data.pr_propose_cid);
							ii->qsig_data.pr_propose_pn = ast_strdup(i->qsig_data.pr_propose_pn);
							ii->qsig_data.pr_propose_active = 1;
						}
					} else 
						cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * QSIG_PATHREPLACEMENT_PROPOSE: no partner channel found (%#x)\n", i->qsig_data.partner_plci);
					

								
					ast_free(i->qsig_data.pr_propose_cid);
					i->qsig_data.pr_propose_cid = NULL;
					ast_free(i->qsig_data.pr_propose_pn);
					i->qsig_data.pr_propose_pn = NULL;
				}
				
				if ((i->qsig_data.pr_propose_cid && i->qsig_data.pr_propose_pn) && i->qsig_data.pr_propose_doinboundbridge) {
					struct ast_channel *chanx;
					struct capi_pvt *ii = capi_find_interface_by_plci(i->qsig_data.partner_plci);
					
					if (ii) {
						cc_qsig_verbose( 1, VERBOSE_PREFIX_4 "  * QSIG_PATHREPLACEMENT_PROPOSE: trying to complete bridge...\n");
						chanx = ast_bridged_channel(i->owner);
						ast_channel_masquerade(ii->owner, chanx);
					}
					
					ast_free(i->qsig_data.pr_propose_cid);
					i->qsig_data.pr_propose_cid = NULL;
					ast_free(i->qsig_data.pr_propose_pn);
					i->qsig_data.pr_propose_pn = NULL;
				}			
				
			}
			break;
		case 0x001e:	/* Progress Indicator */
			break;
		case 0x0027:	/*  Notification Indicator */
			break;
		case 0x0028:	/* DSP */
			break;
		case 0x0029:	/* Date/Time */
			break;
		case 0x0070:	/* Called Party Number */
			break;
		case 0x0074:	/* Redirecting Number */
			break;
		case 0x0076:	/* Redirection Number */
			break;
		case 0x00a1:	/* Sending Complete */
			break;
		case 0x4000:	/* CHARGE in UNITS */
			break;
		case 0x4001:	/* CHARGE in CURRENCY */
			break;
		case 0x8001:	/* ALERTING */
			/* TODO: some checks, if there's any work here */
			if (i->qsig_data.calltransfer_onring) {
				i->qsig_data.calltransfer_onring = 0;
				send_feature_calltransfer(i);
			}
			break;
		case 0x8002:	/* CALL PROCEEDING */
			break;
		case 0x8003:	/* PROGRESS */
			break;
		case 0x8005:	/* SETUP */
			break;
		case 0x8007:	/* CONNECT */
			if (i->qsig_data.calltransfer) {
				i->qsig_data.calltransfer = 0;
				send_feature_calltransfer(i);
			}
			{
				/* handle prior received Path Replacement */
				if ((i->qsig_data.pr_propose_cid && i->qsig_data.pr_propose_pn) && i->qsig_data.pr_propose_sendback) {
					unsigned char fac[CAPI_MAX_FACILITYDATAARRAY_SIZE];
			
					cc_qsig_do_facility(fac, i->owner, NULL, 4, 0);
						
					capi_sendf(NULL, 0, CAPI_INFO_REQ, i->PLCI, get_capi_MessageNumber(),
							"()(()()()s)",
							fac
					);
					
					i->qsig_data.pr_propose_sendback = 0;
 					ast_free(i->qsig_data.pr_propose_cid);
					i->qsig_data.pr_propose_cid = NULL;
 					ast_free(i->qsig_data.pr_propose_pn);
					i->qsig_data.pr_propose_pn = NULL;
					
					i->qsig_data.pr_propose_sentback = 1;
				}
			}

			break;
		case 0x800d:	/* SETUP ACK */
			break;
		case 0x800f:	/* CONNECT ACK */
			{
				unsigned int qsiginvoke;
				qsiginvoke = cc_qsig_handle_capi_facilityind( (unsigned char*) INFO_IND_INFOELEMENT(CMSG), i);
			}
			{
				/* handle outbound Path Replacement */
				if ((i->qsig_data.pr_propose_cid && i->qsig_data.pr_propose_pn) && i->qsig_data.pr_propose_doinboundbridge) {
					unsigned char fac[CAPI_MAX_FACILITYDATAARRAY_SIZE];
			
					cc_qsig_do_facility(fac, i->owner, NULL, 4, 0);
						
					capi_sendf(NULL, 0, CAPI_INFO_REQ, i->PLCI, get_capi_MessageNumber(),
							"()(()()()s)", fac  );
					
					i->qsig_data.pr_propose_sendback = 0;
					ast_free(i->qsig_data.pr_propose_cid);
					i->qsig_data.pr_propose_cid = NULL;
					ast_free(i->qsig_data.pr_propose_pn);
					i->qsig_data.pr_propose_pn = NULL;
					
					i->qsig_data.pr_propose_sentback = 1;
					i->qsig_data.pr_propose_doinboundbridge = 0;

				}
			}
			break;
		case 0x8045:	/* DISCONNECT */
			qsig_cleanup_channel(i);
			
			break;
		case 0x804d:	/* RELEASE */
			break;
		case 0x805a:	/* RELEASE COMPLETE */
			qsig_cleanup_channel(i);

			break;
		case 0x8062:	/* FACILITY */
			break;
		case 0x806e:	/* NOTIFY */
			break;
		case 0x807b:	/* INFORMATION */
			break;
		case 0x807d:	/* STATUS */
			break;
		default:
			break;
	}
  	pbx_capi_qsig_post_handling(i);
	return;

}
