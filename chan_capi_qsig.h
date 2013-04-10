/*
 *  (QSIG)
 *
 *  Implementation of QSIG extensions for chan_capi
 *  
 *  Copyright 2006-2007 (c) Mario Goegel
 *
 *  Mario Goegel <m.goegel@gmx.de>
 *
 * This program is free software and may be modified and
 * distributed under the terms of the GNU Public License.
 */

#ifndef PBX_QSIG_H
#define PBX_QSIG_H

int capiqsigdebug;

#define QSIG_DISABLED		0x00
#define QSIG_ENABLED		0x01		/* shouldn't be used anymore */

#define QSIG_TYPE_ALCATEL_ECMA	0x01		/* use additional Alcatel ECMA */
#define QSIG_TYPE_HICOM_ECMAV2	0x02		/* use additional Hicom ECMA V2 */

#define CAPI_QSIG_WAITEVENT_PRPROPOSE 0x01000000


#define Q932_PROTOCOL_ROSE			0x11	/* X.219 & X.229 */
#define Q932_PROTOCOL_CMIP			0x12	/* Q.941 */
#define Q932_PROTOCOL_ACSE			0x13	/* X.217 & X.227 */
#define Q932_PROTOCOL_GAT			0x16
#define Q932_PROTOCOL_EXTENSIONS	0x1F

#define COMP_TYPE_INVOKE	0xa1		/* Invoke component */
#define COMP_TYPE_DISCR_SS	0x91		/* Supplementary service descriptor - ROSE PROTOCOL */
#define COMP_TYPE_NFE		0xaa		/* Network Facility Extensions (ECMA-165) */
#define COMP_TYPE_APDU_INTERP	0x8b		/* APDU Interpration Type (0 DISCARD, 1 CLEARCALL-IF-UNKNOWN, 2 REJECT-APDU) */
#define COMP_TYPE_RETURN_RESULT	0xA2
#define COMP_TYPE_RETURN_ERROR	0xA3
#define COMP_TYPE_REJECT	0xA4
		
#define APDUINTERPRETATION_IGNORE	0x00
#define APDUINTERPRETATION_CLEARCALL	0x01
#define APDUINTERPRETATION_REJECT	0x02

/* const char* APDU_STR[] = { "IGNORE APDU", "CLEARCALL-IF-UNKNOWN", "REJECT APDU" }; */


		/* ASN.1 Identifier Octet - Data types */
#define ASN1_TYPE_MASK			0x1f
#define ASN1_BOOLEAN			0x01
#define ASN1_INTEGER			0x02
#define ASN1_BITSTRING			0x03
#define ASN1_OCTETSTRING		0x04
#define ASN1_NULL				0x05
#define ASN1_OBJECTIDENTIFIER	0x06
#define ASN1_OBJECTDESCRIPTOR	0x07
#define ASN1_EXTERN				0x08
#define ASN1_REAL				0x09
#define ASN1_ENUMERATED			0x0a
#define ASN1_EMBEDDEDPDV		0x0b
#define ASN1_UTF8STRING			0x0c
#define ASN1_RELATIVEOBJECTID	0x0d
		/* 0x0e & 0x0f are reserved for future ASN.1 editions */
#define ASN1_SEQUENCE			0x10
#define ASN1_SET				0x11
#define ASN1_NUMERICSTRING		0x12
#define ASN1_PRINTABLESTRING	0x13
#define ASN1_TELETEXSTRING		0x14
#define ASN1_IA5STRING			0x16
#define ASN1_UTCTIME			0x17
#define ASN1_GENERALIZEDTIME	0x18

/* ASN.1 Type/Tag Class (bits 7 & 6 of Tag octet) */
#define ASN1_TC_UNIVERSAL	0x00
#define ASN1_TC_APPLICATION	0x40
#define ASN1_TC_CONTEXTSPEC	0x80
#define ASN1_TC_PRIVATE		0xC0

/* ASN.1 Type/Tag Form (bit 5 of Tag octet) */
#define	ASN1_TF_PRIMITVE	0x00
#define ASN1_TF_CONSTRUCTED	0x20		/* field may be a type of sequence or set */

#define CNIP_CALLINGNAME	0x00		/* Name-Types defined in ECMA-164 */
#define CNIP_CALLEDNAME		0x01
#define CNIP_CONNECTEDNAME	0x02
#define CNIP_BUSYNAME		0x03


#define CNIP_NAMEPRESALLOW	0x80
#define CNIP_NAMEPRESRESTR	0xA0
#define CNIP_NAMEPRESUNAVAIL	0xC0

#define CNIP_NAMEUSERPROVIDED	0x00		/* Name is User-provided, unvalidated */
#define CNIP_NAMEUSERPROVIDEDV	0x01		/* Name is User-provided and validated */
		
/* QSIG Operations += 1000 */
#define CCQSIG__ECMA__NAMEPRES	1000		/* Setting an own constant for ECMA Operation/Namepresentation, others will follow */
#define CCQSIG__ECMA__PRPROPOSE	1004		/* Path Replacement Propose */
#define CCQSIG__ECMA__CTCOMPLETE 1012		/* Call Transfer Complete */
#define CCQSIG__ECMA__LEGINFO2	1021		/* LEG INFORMATION2 */
#define CCQSIG__ECMA__LEGINFO3	1022		/* LEG INFORMATION3 */


#define CCQSIG_TIMER_WAIT_PRPROPOSE 1		/* Wait x seconds */

struct rose_component {
	u_int8_t type;
	u_int8_t len;
	u_int8_t data[0];
};

#define GET_COMPONENT(component, idx, ptr, length) \
	if ((idx)+2 > (length)) \
		break; \
	(component) = (struct rose_component*)&((ptr)[idx]); \
	if ((idx)+(component)->len+2 > (length)) { \
		if ((component)->len != ASN1_LEN_INDEF) \
			pri_message(pri, "Length (%d) of 0x%X component is too long\n", (component)->len, (component)->type); \
}

#define NEXT_COMPONENT(component, idx) \
	(idx) += (component)->len + 2

#define SUB_COMPONENT(component, idx) \
	(idx) += 2

#define CHECK_COMPONENT(component, comptype, message) \
	if ((component)->type && ((component)->type & ASN1_TYPE_MASK) != (comptype)) { \
		pri_message(pri, (message), (component)->type); \
		asn1_dump(pri, (component), (component)->len+2); \
		break; \
}
	
#define ASN1_GET_INTEGER(component, variable) \
	do { \
		int comp_idx; \
		(variable) = 0; \
		for (comp_idx = 0; comp_idx < (component)->len; ++comp_idx) \
			(variable) = ((variable) << 8) | (component)->data[comp_idx]; \
} while (0)

#define ASN1_FIXUP_LEN(component, size) \
	do { \
		if ((component)->len == ASN1_LEN_INDEF) \
			size += 2; \
} while (0)

#define ASN1_ADD_SIMPLE(component, comptype, ptr, idx) \
	do { \
		(component) = (struct rose_component *)&((ptr)[(idx)]); \
		(component)->type = (comptype); \
		(component)->len = 0; \
		(idx) += 2; \
} while (0)

#define ASN1_ADD_BYTECOMP(component, comptype, ptr, idx, value) \
	do { \
		(component) = (struct rose_component *)&((ptr)[(idx)]); \
		(component)->type = (comptype); \
		(component)->len = 1; \
		(component)->data[0] = (value); \
		(idx) += 3; \
} while (0)

#define ASN1_ADD_WORDCOMP(component, comptype, ptr, idx, value) \
	do { \
		int __val = (value); \
		int __i = 0; \
		(component) = (struct rose_component *)&((ptr)[(idx)]); \
		(component)->type = (comptype); \
		if ((__val >> 24)) \
			(component)->data[__i++] = (__val >> 24) & 0xff; \
		if ((__val >> 16)) \
			(component)->data[__i++] = (__val >> 16) & 0xff; \
		if ((__val >> 8)) \
			(component)->data[__i++] = (__val >> 8) & 0xff; \
		(component)->data[__i++] = __val & 0xff; \
		(component)->len = __i; \
		(idx) += 2 + __i; \
} while (0)

#define ASN1_PUSH(stack, stackpointer, component) \
	(stack)[(stackpointer)++] = (component)

#define ASN1_FIXUP(stack, stackpointer, data, idx) \
	do { \
		--(stackpointer); \
		(stack)[(stackpointer)]->len = (unsigned char *)&((data)[(idx)]) - (unsigned char *)(stack)[(stackpointer)] - 2; \
} while (0)


#define free_null(x)	{ ast_free(x); x = NULL; }

/* Common QSIG structs */

/*
 * INVOKE Data struct, contains data for further operations
 */
struct cc_qsig_invokedata {
	int len;		/* invoke length */
	int offset;		/* where does the invoke start in facility array */
	int id;			/* id from sent Invoke Number */
	int apdu_interpr;	/* What To Do with unknown Operation? */
	int descr_type;		/* component descriptor is of ASN.1 Datatype (0x02 Integer, 0x06 Object Identifier) */
	int type;		/* when component is Integer */
	int oid_len;
	unsigned char oid_bin[20];	/* when component is Object Identifier then save here the binary oid */
	int datalen;			/* invoke struct len */
	unsigned char data[255];	/* invoke */
};

/*
 * NFE Entity Address - contains destination informations for following INVOKE's
 */
struct cc_qsig_entityaddr {	/* In case of AnyPINX */
	int partynum;		/* private,public,etc a5=private */
	int ton;		/* Type of Number */
	unsigned char *num;		/* EntityNumber */
};
	
/*
 * Network Facility Extensions struct - to which pbx does the INVOKE belong to
 */
struct cc_qsig_nfe {
	int src_entity;		/* Call is coming from PBX (End|Any) */
	int dst_entity;		/* Call destination is */
	struct cc_qsig_entityaddr src_addr;	/* additional infos (PBX identifier) */
	struct cc_qsig_entityaddr dst_addr;	/* same here for destination */
};


/*
 * prototypes
 */

/*
 ***  QSIG Core Functions 
 */
extern void cc_qsig_verbose(int c_d, char *text, ...);

extern int cc_qsig_build_facility_struct(unsigned char * buf, unsigned int *idx, int protocolvar, int apdu_interpr, struct cc_qsig_nfe *nfe);
extern int cc_qsig_add_invoke(unsigned char * buf, unsigned int *idx, struct cc_qsig_invokedata *invoke, struct capi_pvt *i);

extern unsigned int cc_qsig_asn1_get_string(unsigned char *buf, int buflen, unsigned char *data);
extern unsigned int cc_qsig_asn1_get_integer(unsigned char *data, int *idx);
extern unsigned char *cc_qsig_asn1_oid2str(unsigned char *data, int size);
extern int cc_qsig_asn1_add_string2(unsigned char asn1_type, void *data, int len, int max_len, void *src, int src_len);
extern unsigned int cc_qsig_asn1_add_string(unsigned char *buf, int *idx, char *data, int datalen);
extern unsigned int cc_qsig_asn1_add_integer(unsigned char *buf, int *idx, int value);

extern signed int cc_qsig_asn1_check_ecma_isdn_oid(unsigned char *data, int len);
extern unsigned int cc_qsig_check_facility(unsigned char *data, int *idx, int *apduval, int protocol);
extern signed int cc_qsig_check_invoke(unsigned char *data, int *idx);
extern signed int cc_qsig_get_invokeid(unsigned char *data, int *idx, struct cc_qsig_invokedata *invoke);
extern signed int cc_qsig_fill_invokestruct(unsigned char *data, int *idx, struct cc_qsig_invokedata *invoke, int apduval);
extern unsigned int cc_qsig_handle_capiind(unsigned char *data, struct capi_pvt *i);
extern unsigned int cc_qsig_handle_capi_facilityind(unsigned char *data, struct capi_pvt *i);
extern unsigned int cc_qsig_add_call_setup_data(unsigned char *data, struct capi_pvt *i, struct  ast_channel *c);
extern unsigned int cc_qsig_add_call_answer_data(unsigned char *data, struct capi_pvt *i, struct  ast_channel *c);
extern unsigned int cc_qsig_add_call_alert_data(unsigned char *data, struct capi_pvt *i, struct  ast_channel *c);
extern unsigned int cc_qsig_add_call_facility_data(unsigned char *data, struct capi_pvt *i, int facility);

extern signed int cc_qsig_identifyinvoke(struct cc_qsig_invokedata *invoke, int protocol);
extern unsigned int cc_qsig_handle_invokeoperation(int invokeident, struct cc_qsig_invokedata *invoke, struct capi_pvt *i);

extern unsigned int cc_qsig_do_facility(unsigned char *fac, struct  ast_channel *c, char *param, unsigned int factype, int info1);

extern int pbx_capi_qsig_getplci(struct ast_channel *c, char *param);
extern int pbx_capi_qsig_ssct(struct ast_channel *c, char *param);
extern int pbx_capi_qsig_ct(struct ast_channel *c, char *param);
extern int pbx_capi_qsig_callmark(struct ast_channel *c, char *param);
extern int pbx_capi_qsig_bridge(struct capi_pvt *i0, struct capi_pvt *i1);
extern int pbx_capi_qsig_sendtext(struct ast_channel *c, const char *text);


extern void cc_qsig_interface_init(struct cc_capi_conf *conf, struct capi_pvt *tmp);
extern void cc_pbx_qsig_conf_interface_value(struct cc_capi_conf *conf, struct ast_variable *v);
extern void interface_cleanup_qsig(struct capi_pvt *i);
extern void pbx_capi_qsig_unload_module(struct capi_pvt *i);
extern void pbx_capi_qsig_handle_info_indication(_cmsg *CMSG, unsigned int PLCI, unsigned int NCCI, struct capi_pvt *i);


#endif
