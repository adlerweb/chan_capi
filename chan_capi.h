/*
 * An implementation of Common ISDN API 2.0 for Asterisk
 *
 * Copyright (C) 2005-2009 Cytronics & Melware
 *
 * Armin Schindler <armin@melware.de>
 * 
 * Reworked, but based on the work of
 * Copyright (C) 2002-2005 Junghanns.NET GmbH
 *
 * Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
 *
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 */

#include "config.h"

#ifdef CC_AST_HAS_VERSION_1_4
#include <asterisk.h>
#endif

#include <asterisk/lock.h>
#include <asterisk/frame.h>
#include <asterisk/channel.h>
#include <asterisk/logger.h>
#include <asterisk/module.h>
#include <asterisk/pbx.h>
#include <asterisk/config.h>
#include <asterisk/options.h>
#include <asterisk/features.h>
#include <asterisk/utils.h>
#include <asterisk/cli.h>
#ifdef CC_AST_HAS_RTP_ENGINE_H
#include <asterisk/rtp_engine.h>
#else
#include <asterisk/rtp.h>
#endif
#include <asterisk/causes.h>
#include <asterisk/strings.h>
#include <asterisk/dsp.h>
#include <asterisk/devicestate.h>
#ifdef CC_AST_HAS_VERSION_1_4
#include "asterisk/abstract_jb.h"
#endif
#include "asterisk/musiconhold.h"
#include "dlist.h"
#include "chan_capi_fmt.h"
 
#ifndef _PBX_CAPI_H
#define _PBX_CAPI_H

#ifdef DIVA_STREAMING
struct _diva_stream_scheduling_entry;
#endif
struct _pbx_capi_conference_bridge;

#define CAPI_MAX_CONTROLLERS             64
#define CAPI_MAX_B3_BLOCKS                7

/* was : 130 bytes Alaw = 16.25 ms audio not suitable for VoIP */
/* now : 160 bytes Alaw = 20 ms audio */
/* now : 640 bytes slinear 16000Hz = 20 ms audio */
/* you can tune this to your need. higher value == more latency */
#define CAPI_MAX_B3_BLOCK_SIZE          160

#define ALL_SERVICES             0x1FFF03FF

#define CAPI_ISDNMODE_MSN                 0
#define CAPI_ISDNMODE_DID                 1

#define RTP_HEADER_SIZE                  12

#define CAPI_MAX_FACILITYDATAARRAY_SIZE 300

#define COMMANDSEPARATOR "|,"

#ifdef CC_AST_HAS_FORMAT_T
typedef format_t cc_format_t;
#else
typedef int cc_format_t;
#endif

/* some helper functions */
static inline void write_capi_word(void *m, unsigned short val)
{
	((unsigned char *)m)[0] = val & 0xff;
	((unsigned char *)m)[1] = (val >> 8) & 0xff;
}
static inline unsigned short read_capi_word(const void *m)
{
	unsigned short val;

	val = ((const unsigned char *)m)[0] | (((const unsigned char *)m)[1] << 8);	
	return (val);
}
static inline void write_capi_dword(void *m, unsigned int val)
{
	((unsigned char *)m)[0] = val & 0xff;
	((unsigned char *)m)[1] = (val >> 8) & 0xff;
	((unsigned char *)m)[2] = (val >> 16) & 0xff;
	((unsigned char *)m)[3] = (val >> 24) & 0xff;
}
static inline unsigned int read_capi_dword(const void *m)
{
	unsigned int val;

	val = ((const unsigned char *)m)[0] | (((const unsigned char *)m)[1] << 8) |	
	      (((const unsigned char *)m)[2] << 16) | (((const unsigned char *)m)[3] << 24);	
	return (val);
}

/*
 * global name for messages and commands
 */
#define CC_MESSAGE_NAME "capi"
#define CC_MESSAGE_BIGNAME "CAPI"

/*
 * define some private functions
 */
#define cc_mutex_t                ast_mutex_t
#define cc_mutex_init             ast_mutex_init
#define cc_mutex_lock(x)          ast_mutex_lock(x)
#define cc_mutex_unlock(x)        ast_mutex_unlock(x)
#define cc_mutex_destroy(x)       ast_mutex_destroy(x)
#define cc_log(x...)              ast_log(x)
#define cc_pbx_verbose(x...)      ast_verbose(x)
#define cc_copy_string(dst, src, size)  ast_copy_string(dst, src, size)

#ifndef AST_MUTEX_DEFINE_STATIC
#define AST_MUTEX_DEFINE_STATIC(mutex)		\
	static cc_mutex_t mutex = AST_MUTEX_INITIALIZER
#endif

/*
 * definitions for nice compatibility
 */
#define CC_CHANNEL_PVT(c) (c)->tech_pvt
#define CC_BRIDGE_RETURN enum ast_bridge_result

#ifdef CC_AST_HAS_UNION_DATA_IN_FRAME
#define FRAME_DATA_PTR data.ptr
#else
#define FRAME_DATA_PTR data
#endif

#ifdef CC_AST_HAS_UNION_SUBCLASS_IN_FRAME
#define FRAME_SUBCLASS_INTEGER(x) x.integer

#ifndef CC_AST_HAS_VERSION_10_0
#define SET_FRAME_SUBCLASS_CODEC(__a__,__b__) do {(__a__).codec = (__b__); }while(0)
#define GET_FRAME_SUBCLASS_CODEC(__a__) (__a__).codec
#else
#define SET_FRAME_SUBCLASS_CODEC(__a__,__b__) do{ ast_format_from_old_bitfield(&(__a__).format, __b__); } while(0)
#define GET_FRAME_SUBCLASS_CODEC(__a__)       ast_format_to_old_bitfield(&(__a__).format)
#endif

#else
#define FRAME_SUBCLASS_INTEGER(x) x
#define SET_FRAME_SUBCLASS_CODEC(__a__, __b__) do { (__a__) = (__b__); }while(0)
#define GET_FRAME_SUBCLASS_CODEC(__a__) __a__
#endif

#ifndef CC_AST_HAS_CHANNEL_RELEASE
#define ast_channel_release(x) ast_channel_free(x)
#endif

#ifndef CC_AST_HAS_AST_DEVSTATE2STR
#define ast_devstate2str(x) devstate2str(x)
#endif

/* */
#define return_on_no_interface(x)                                       \
	if (!i) {                                                       \
		cc_verbose(4, 1, "CAPI: %s no interface for PLCI=%#x\n", x, PLCI);   \
		return;                                                 \
	}

/*
 * B protocol settings
 */
#define CC_BPROTO_TRANSPARENT   0
#define CC_BPROTO_FAXG3         1
#define CC_BPROTO_RTP           2
#define CC_BPROTO_VOCODER       3
#define CC_BPROTO_FAX3_BASIC    4

/* FAX Resolutions */
#define FAX_STANDARD_RESOLUTION         0
#define FAX_HIGH_RESOLUTION             1

/* FAX Formats */
#define FAX_SFF_FORMAT                  0
#define FAX_PLAIN_FORMAT                1
#define FAX_PCX_FORMAT                  2
#define FAX_DCX_FORMAT                  3
#define FAX_TIFF_FORMAT                 4
#define FAX_ASCII_FORMAT                5
#define FAX_EXTENDED_ASCII_FORMAT       6
#define FAX_BINARY_FILE_TRANSFER_FORMAT 7
#define FAX_NATIVE_FILE_TRANSFER_FORMAT 8

/* Fax struct */
struct fax3proto3 {
	unsigned char len;
	unsigned short resolution;
	unsigned short format;
	unsigned char Infos[100];
} __attribute__((__packed__));

typedef struct fax3proto3 B3_PROTO_FAXG3;

/* duration in ms for sending and detecting dtmfs */
#define CAPI_DTMF_DURATION              0x50

#define CAPI_NATIONAL_PREF               "0"
#define CAPI_INTERNAT_PREF              "00"
#define CAPI_SUBSCRIBER_PREF            ""

#define ECHO_TX_COUNT                   5 /* 5 x 20ms = 100ms */
#define ECHO_EFFECTIVE_TX_COUNT         3 /* 2 x 20ms = 40ms == 40-100ms  ... ignore first 40ms */
#define ECHO_TXRX_RATIO                 2.3 /* if( rx < (txavg/ECHO_TXRX_RATIO) ) rx=0; */

#define FACILITYSELECTOR_DTMF              0x0001
#define FACILITYSELECTOR_SUPPLEMENTARY     0x0003
#define FACILITYSELECTOR_LINE_INTERCONNECT 0x0005
#define FACILITYSELECTOR_ECHO_CANCEL       0x0008
#define PRIV_SELECTOR_DTMF_ONDATA          0x00fa
#define FACILITYSELECTOR_FAX_OVER_IP       0x00fd
#define FACILITYSELECTOR_VOICE_OVER_IP     0x00fe

#define EC_FUNCTION_ENABLE              1   
#define EC_FUNCTION_DISABLE             2
#define EC_FUNCTION_FREEZE              3   
#define EC_FUNCTION_RESUME              4
#define EC_FUNCTION_RESET               5   
#define EC_OPTION_DISABLE_NEVER         0   
#define EC_OPTION_DISABLE_G165          (1<<2)
#define EC_OPTION_DISABLE_G164_OR_G165  (1<<1 | 1<<2)
#define EC_DEFAULT_TAIL                 0 /* maximum */

/*
	EC path mask
	*/
#define EC_ECHOCANCEL_PATH_IFC          1 /* Default, activate EC for E.1/T.1/S0 only */
#define EC_ECHOCANCEL_PATH_IP           2 /* Activate EC for IP */
#define EC_ECHOCANCEL_PATH_BITS         (EC_ECHOCANCEL_PATH_IFC | EC_ECHOCANCEL_PATH_IP)

/*
	Control EC on transit connectionss
	*/
#define EC_ECHOCANCEL_TRANSIT_OFF       0 /* EC deactivated on transit connection, default */
#define EC_ECHOCANCEL_TRANSIT_A         1 /* EC activated on side A of transsit connection */
#define EC_ECHOCANCEL_TRANSIT_B         2 /* EC activated on side B of transsit connection */
#define EC_ECHOCANCEL_TRANSIT_AB        (EC_ECHOCANCEL_TRANSIT_A | EC_ECHOCANCEL_TRANSIT_B)
#define EC_ECHOCANCEL_TRANSIT_BITS      (EC_ECHOCANCEL_TRANSIT_A | EC_ECHOCANCEL_TRANSIT_B)


#define CC_HOLDTYPE_LOCAL               0
#define CC_HOLDTYPE_HOLD                1
#define CC_HOLDTYPE_NOTIFY              2

/*
 * state combination for a normal incoming call:
 * DIS -> ALERT -> CON -> DIS
 *
 * outgoing call:
 * DIS -> CONP -> CON -> DIS
 */

#define CAPI_STATE_ALERTING             1
#define CAPI_STATE_CONNECTED            2

#define CAPI_STATE_DISCONNECTING        3
#define CAPI_STATE_DISCONNECTED         4

#define CAPI_STATE_CONNECTPENDING       5
#define CAPI_STATE_ANSWERING            6
#define CAPI_STATE_DID                  7
#define CAPI_STATE_INCALL               8

#define CAPI_STATE_ONHOLD              10

#define CAPI_B3_DONT                    0
#define CAPI_B3_ALWAYS                  1
#define CAPI_B3_ON_SUCCESS              2

#define CAPI_MAX_STRING              2048

#define CAPI_FAX_DETECT_INCOMING      0x00000001
#define CAPI_FAX_DETECT_OUTGOING      0x00000002
#define CAPI_FAX_STATE_HANDLED        0x00010000
#define CAPI_FAX_STATE_ACTIVE         0x00020000
#define CAPI_FAX_STATE_ERROR          0x00040000
#define CAPI_FAX_STATE_SENDMODE       0x00080000
#define CAPI_FAX_STATE_CONN           0x00100000
#define CAPI_FAX_STATE_MASK           0xffff0000

struct cc_capi_gains {
	unsigned char txgains[256];
	unsigned char rxgains[256];
};

#define CAPI_ISDN_STATE_SETUP         0x00000001
#define CAPI_ISDN_STATE_SETUP_ACK     0x00000002
#define CAPI_ISDN_STATE_HOLD          0x00000004
#define CAPI_ISDN_STATE_ECT           0x00000008
#define CAPI_ISDN_STATE_PROGRESS      0x00000010
#define CAPI_ISDN_STATE_LI            0x00000020
#define CAPI_ISDN_STATE_DISCONNECT    0x00000040
#define CAPI_ISDN_STATE_DID           0x00000080
#define CAPI_ISDN_STATE_B3_PEND       0x00000100
#define CAPI_ISDN_STATE_B3_UP         0x00000200
#define CAPI_ISDN_STATE_B3_CHANGE     0x00000400
#define CAPI_ISDN_STATE_RTP           0x00000800
#define CAPI_ISDN_STATE_HANGUP        0x00001000
#define CAPI_ISDN_STATE_EC            0x00002000
#define CAPI_ISDN_STATE_DTMF          0x00004000
#define CAPI_ISDN_STATE_B3_SELECT     0x00008000
#define CAPI_ISDN_STATE_ISDNPROGRESS  0x00010000
#define CAPI_ISDN_STATE_3PTY          0x10000000
#define CAPI_ISDN_STATE_PBX_DONT      0x40000000
#define CAPI_ISDN_STATE_PBX           0x80000000

#define CAPI_ISDN_STATE2_PROCEEDING         0x00000001
#define CAPI_ISDN_STATE2_PROCEEDING_PENDING 0x00000002

#define CAPI_CHANNELTYPE_B            0
#define CAPI_CHANNELTYPE_D            1
#define CAPI_CHANNELTYPE_NULL         2

#define CAPI_RESOURCE_PLCI_NULL       0
#define CAPI_RESOURCE_PLCI_DATA       1
#define CAPI_RESOURCE_PLCI_LINE       2

/* the lower word is reserved for capi commands */
#define CAPI_WAITEVENT_B3_UP          0x00010000
#define CAPI_WAITEVENT_B3_DOWN        0x00020000
#define CAPI_WAITEVENT_ANSWER_FINISH  0x00030000
#define CAPI_WAITEVENT_HOLD_IND       0x00040000
#define CAPI_WAITEVENT_ECT_IND        0x00050000

/* Features and settings of current connection */
#define CAPI_FSETTING_STAYONLINE      0x00000001
#define CAPI_FSETTING_EARLY_BRIDGE    0x00000002

/* Private qsig data for capi device */
struct cc_qsig_data {
	int calltransfer_active;
	int calltransfer;
	int calltransfer_onring;
	unsigned int callmark;
	
	char *dnameid;

	/* Path Replacement */
	int pr_propose_sendback; /* send back an prior received PR PROPOSE on Connect */
	int pr_propose_sentback; /* set to 1 after sending an PR PROPOSE */
	int pr_propose_active;
	int pr_propose_doinboundbridge; /* We have to to bridge a call back to asterisk */
	char *pr_propose_cid;	/* Call identity */
	char *pr_propose_pn;	/* Party Number */
	
	char if_pr_propose_pn[AST_MAX_EXTENSION];	/* configured interface Party Number */
	
	/* Partner Channel - needed for many features */
	struct capi_pvt *partner_ch;
	unsigned int partner_plci;
	ast_cond_t event_trigger;
	unsigned int waitevent;
};

/* ! Private data for a capi device */
struct capi_pvt {
	cc_mutex_t lock;

	int readerfd;
	int writerfd;
	struct ast_frame f;
	unsigned char frame_data[CAPI_MAX_B3_BLOCK_SIZE + AST_FRIENDLY_OFFSET + RTP_HEADER_SIZE];

	ast_cond_t event_trigger;
	unsigned int waitevent;

	char name[CAPI_MAX_STRING];
	char vname[CAPI_MAX_STRING];
	unsigned char tmpbuf[CAPI_MAX_STRING];

	/*! Channel who used us, possibly NULL */
	struct ast_channel *used;		
	/*! Channel we belong to, possibly NULL */
	struct ast_channel *owner;		
	/*! Channel who called us, possibly NULL */
	struct ast_channel *peer;		
	/*! Set if structure is reserved */
	volatile int reserved;
	
	/* capi message number */
	_cword MessageNumber;	
	unsigned int NCCI;
	unsigned int PLCI;
	/* on which controller we do live */
	int controller;
	
	/* send buffer */
	unsigned char send_buffer[CAPI_MAX_B3_BLOCKS *
		(CAPI_MAX_B3_BLOCK_SIZE + AST_FRIENDLY_OFFSET)];
	unsigned short send_buffer_handle;

	/* receive buffer */
	unsigned char rec_buffer[CAPI_MAX_B3_BLOCK_SIZE + AST_FRIENDLY_OFFSET + RTP_HEADER_SIZE];

	/* current state */
	int state;

	/* the state of the line */
	unsigned int isdnstate;
	unsigned int isdnstate2;
	int cause;

	/* which b-protocol is active */
	int bproto;

	char context[AST_MAX_EXTENSION];
	/*! Multiple Subscriber Number we listen to (, seperated list) */
	char incomingmsn[CAPI_MAX_STRING];	
	/*! Prefix to Build CID */
	char prefix[AST_MAX_EXTENSION];	
	/* the default caller id */
	char defaultcid[CAPI_MAX_STRING];

	/*! Caller ID if available */
	char cid[AST_MAX_EXTENSION];	
	/*! Dialed Number if available */
	char dnid[AST_MAX_EXTENSION];
	/* callerid type of number */
	int cid_ton;

	char accountcode[20];	
	int amaflags;

	ast_group_t callgroup;
	ast_group_t pickupgroup;
	ast_group_t group;
	
	ast_group_t transfergroup;

	/* language */
	char language[MAX_LANGUAGE];	

	/* additional numbers to dial */
	int doOverlap;
	char overlapdigits[AST_MAX_EXTENSION];

	int calledPartyIsISDN;
	/* this is an outgoing channel */
	int outgoing;
	/* should we do early B3 on this interface? */
	int doB3;
	/* store plci here for the call that is onhold */
	unsigned int onholdPLCI;
	/* do software dtmf detection */
	int doDTMF;
	/* CAPI echo cancellation */
	int doEC;
	int doEC_global;
	int ecOption;
	int ecTail;
	int ecSelector;
	/* isdnmode MSN or DID */
	int isdnmode;
	/* NT-mode */
	int ntmode;
	/* Answer before getting digits? */
	int immediate;
	/* which holdtype */
	int holdtype;
	int doholdtype;
	/* line interconnect allowed */
	int bridge;
	/* channeltype */
	int channeltype;

	/* Common ISDN Profile (CIP) */
	int cip;
	unsigned short transfercapability;

	/* Features and settings of current connection */
	unsigned int fsetting;
	
	/* if not null, receiving a fax */
	FILE *fFax;
	/* Fax status */
	unsigned int FaxState;
	/* Window for fax detection */
	unsigned int faxdetecttime;
	/* custom fax context,exten,prio */
	char faxcontext[AST_MAX_EXTENSION+1];
	char faxexten[AST_MAX_EXTENSION+1];
	int faxpriority;

	/* handle for CCBS/CCNR callback */
	unsigned int ccbsnrhandle;

	/* not all codecs supply frames in nice 160 byte chunks */
	struct ast_smoother *smoother;

	/* outgoing queue count */
	int B3q;
	int B3count;

	/* do ECHO SURPRESSION */
	int ES;
	int doES;
	short txavg[ECHO_TX_COUNT];
	float rxmin;
	float txmin;

	unsigned short divaAudioFlags;
	unsigned short divaDataStubAudioFlags;
	unsigned short divaDigitalRxGain;
	float divaDigitalRxGainDB;
	unsigned short divaDigitalTxGain;
	float divaDigitalTxGainDB;
	unsigned short rxPitch;
	unsigned short txPitch;
	char special_tone_extension[AST_MAX_EXTENSION+1];

	char   channel_command_digits[AST_MAX_EXTENSION+1];
	time_t channel_command_timestamp;
	int channel_command_digit;
	int command_pass_digits;
	diva_entity_queue_t channel_command_q;

#ifdef CC_AST_HAS_VERSION_1_4
	struct ast_jb_conf jbconf;
	char mohinterpret[MAX_MUSICCLASS];
#endif
	
	struct cc_capi_gains g;

	float txgain;
	float rxgain;
	struct ast_dsp *vad;

	unsigned int reason;
	unsigned int reasonb3;

	/* deferred tasks */
	time_t whentohangup;
	time_t whentoqueuehangup;
	time_t whentoretrieve;

	/* RTP */
#ifdef CC_AST_HAS_RTP_ENGINE_H
	struct ast_rtp_instance *rtp;
#else
	struct ast_rtp *rtp;
#endif
	cc_format_t capability;
	int rtpcodec;
	int codec;
	unsigned int timestamp;

	/* Q.SIG features */
	int qsigfeat;
	int divaqsig;
	struct cc_qsig_data qsig_data;

	/* Resource PLCI data */
	int resource_plci_type; /* NULL PLCI, DATA, LINE */

	/* Resource PLCI line if data */
	struct capi_pvt *line_plci;
	/* Resource PLCI data data if line */
	struct capi_pvt *data_plci;

#ifdef DIVA_STREAMING
	struct _diva_stream_scheduling_entry* diva_stream_entry;
#endif
	/* Connection between two conference rooms. NULL PLCI */
	int virtualBridgePeer;
	struct capi_pvt *bridgePeer;

	/*! Next channel in list */
	struct capi_pvt *next;
};

struct cc_capi_profile {
	unsigned short ncontrollers;
	unsigned short nbchannels;
	unsigned char globaloptions;
	unsigned char globaloptions2;
	unsigned char globaloptions3;
	unsigned char globaloptions4;
	unsigned int b1protocols;
	unsigned int b2protocols;
	unsigned int b3protocols;
	unsigned int reserved3[6];
	unsigned int manufacturer[5];
} __attribute__((__packed__));

struct cc_capi_qsig_conf {
	char if_pr_propose_pn[AST_MAX_EXTENSION];
};

struct cc_capi_conf {
	char name[CAPI_MAX_STRING];	
	char language[MAX_LANGUAGE];
	char incomingmsn[CAPI_MAX_STRING];
	char defaultcid[CAPI_MAX_STRING];
	char context[AST_MAX_EXTENSION];
	char controllerstr[CAPI_MAX_STRING];
	char prefix[AST_MAX_EXTENSION];
	char accountcode[20];
	int devices;
	int softdtmf;
	int echocancel;
	int ecoption;
	int ectail;
	int ecnlp;
	int ecSelector;
	int isdnmode;
	int ntmode;
	int immediate;
	int holdtype;
	int es;
	int bridge;
	int amaflags;
	int qsigfeat;
	int divaqsig;
	struct cc_capi_qsig_conf qsigconf;
	unsigned int faxsetting;
	unsigned int faxdetecttime;
	/* custom fax context,exten,prio */
	char faxcontext[AST_MAX_EXTENSION+1];
	char faxexten[AST_MAX_EXTENSION+1];
	int faxpriority;
	ast_group_t callgroup;
	ast_group_t pickupgroup;
	ast_group_t group;
	ast_group_t transfergroup;
	float rxgain;
	float txgain;
	struct ast_codec_pref prefs;
#ifdef CC_AST_HAS_FORMAT_T
	format_t capability;
#else
	int capability;
#endif
#ifdef CC_AST_HAS_VERSION_1_4
	struct ast_jb_conf jbconf;
	char mohinterpret[MAX_MUSICCLASS];
#endif
	int echocancelpath;
	int econtransitconn;

	int mwifacptynrtype;
	int mwifacptynrton;
	int mwifacptynrpres;
	int mwibasicservice;
	int mwiinvocation;

	char* mwimailbox;

	int hlimit;
	int slimit;
};

struct cc_capi_controller;
struct _cc_capi_mwi_mailbox;
typedef struct _cc_capi_mwi_mailbox {
	AST_LIST_ENTRY(_cc_capi_mwi_mailbox) link;
	const struct cc_capi_controller *controller;
	unsigned short basicService;
	unsigned short invocationMode;
	unsigned char *mailboxNumber;
	char          *mailboxContext;
	unsigned char *controllingUserNumber;
	unsigned char *controllingUserProvidedNumber;
#if defined(CC_AST_HAS_EVENT_MWI)
	struct ast_event_sub* mwiSubscribtion;
#else
	void* mwiSubscribtion;
#endif
} cc_capi_mwi_mailbox_t;

struct cc_capi_controller {
	/* which controller is this? */
	int controller;
	/* is this controller used? */
	int used;
	/* how many bchans? */
	int nbchannels;
	/* free bchans */
	int nfreebchannels;
	/* Controller considered BUSY amount if free channels below
		of this level */
	int nfreebchannelsHardThr;
	/* If amount of free channels is below this level then
		try to allocate call on other controler in group
		where this level is not reached or difference is less */
	int nfreebchannelsSoftThr;
	/* features: */
	int broadband;
	int dtmf;
	int echocancel;
	int sservices;	/* supplementray services */
	int lineinterconnect;
	/* supported sservices: */
	int holdretrieve;
	int terminalportability;
	int ECT;
	int threePTY;
	int CF;
	int CD;
	int MCID;
	int CCBS;
	int MWI;
	int CCNR;
	int CONF;
	/* RTP */
	int rtpcodec;

	int divaExtendedFeaturesAvailable;
	int ecPath;
	int ecOnTransit;
	int fax_t30_extended;
#ifdef DIVA_STREAMING
	int divaStreaming;
#endif
	AST_LIST_HEAD_NOLOCK(, _cc_capi_mwi_mailbox) mwiSubscribtions;
#ifdef DIVA_STATUS
	int interfaceState;
	int hwState;
#endif
};

/* ETSI 300 102-1 Numbering Plans */
#define CAPI_ETSI_NPLAN_SUBSCRIBER              0x40
#define CAPI_ETSI_NPLAN_NATIONAL                0x20
#define CAPI_ETSI_NPLAN_INTERNAT                0x10

/* Common ISDN Profiles (CIP) */
#define CAPI_CIPI_SPEECH                        0x01
#define CAPI_CIPI_DIGITAL                       0x02
#define CAPI_CIPI_RESTRICTED_DIGITAL            0x03
#define CAPI_CIPI_3K1AUDIO                      0x04
#define CAPI_CIPI_7KAUDIO                       0x05
#define CAPI_CIPI_VIDEO                         0x06
#define CAPI_CIPI_PACKET_MODE                   0x07
#define CAPI_CIPI_56KBIT_RATE_ADAPTION          0x08
#define CAPI_CIPI_DIGITAL_W_TONES               0x09
#define CAPI_CIPI_TELEPHONY                     0x10
#define CAPI_CIPI_FAX_G2_3                      0x11
#define CAPI_CIPI_FAX_G4C1                      0x12
#define CAPI_CIPI_FAX_G4C2_3                    0x13
#define CAPI_CIPI_TELETEX_PROCESSABLE           0x14
#define CAPI_CIPI_TELETEX_BASIC                 0x15
#define CAPI_CIPI_VIDEOTEX                      0x16
#define CAPI_CIPI_TELEX                         0x17
#define CAPI_CIPI_X400                          0x18
#define CAPI_CIPI_X200                          0x19
#define CAPI_CIPI_7K_TELEPHONY                  0x1a
#define CAPI_CIPI_VIDEO_TELEPHONY_C1            0x1b
#define CAPI_CIPI_VIDEO_TELEPHONY_C2            0x1c

/* Transfer capabilities */
#define PRI_TRANS_CAP_SPEECH                    0x00
#define PRI_TRANS_CAP_DIGITAL                   0x08
#define PRI_TRANS_CAP_RESTRICTED_DIGITAL        0x09
#define PRI_TRANS_CAP_3K1AUDIO                  0x10
#define PRI_TRANS_CAP_DIGITAL_W_TONES           0x11
#define PRI_TRANS_CAP_VIDEO                     0x18

/*
 * prototypes
 */
extern
#ifndef CC_AST_HAS_VERSION_10_0
const
#endif
struct ast_channel_tech capi_tech;
#ifdef CC_AST_HAS_FORMAT_T
extern format_t capi_capability;
#else
extern int capi_capability;
#endif
extern unsigned capi_ApplID;
extern struct capi_pvt *capi_iflist;
extern void cc_start_b3(struct capi_pvt *i);
extern unsigned char capi_tcap_is_digital(unsigned short tcap);
extern void capi_queue_cause_control(struct capi_pvt *i, int control);
extern void capidev_handle_connection_conf(struct capi_pvt **i, unsigned int PLCI,
    unsigned short wInfo, unsigned short wMsgNum, struct ast_channel** interface_owner);
extern void capi_wait_for_answered(struct capi_pvt *i);
extern int capi_wait_for_b3_up(struct capi_pvt *i);
extern void capi_activehangup(struct capi_pvt *i, int state);
extern void capi_gains(struct cc_capi_gains *g, float rxgain, float txgain);
#ifdef CC_AST_HAS_VERSION_1_6
extern char chatinfo_usage[];
#endif

typedef int (*pbx_capi_command_proc_t)(struct ast_channel *, char *);
pbx_capi_command_proc_t pbx_capi_lockup_command_by_name(const char* name);
/*!
 * \brief returns list of supported by this controller RTP codecs
 */
cc_format_t pbx_capi_get_controller_codecs(int controller);
_cstruct diva_get_b1_conf(struct capi_pvt *i);
/*!
	\brief &capi_controllers[controller]
	*/
const struct cc_capi_controller *pbx_capi_get_controller(int controller);
/*!
	\brief capi_num_controllers
	*/
int pbx_capi_get_num_controllers(void);
/*!
	\brief tdesc
	*/
const char* pbx_capi_get_module_description(void);
/*!
	\brief cc_mutex_lock(&iflock)
	*/
void pbx_capi_lock_interfaces(void);
/*!
	\brief cc_mutex_unlock(&iflock)
	*/
void pbx_capi_unlock_interfaces(void);
/*!
	\brief Exec cappicommand using CLI
 */
int pbx_capi_cli_exec_capicommand(struct ast_channel *chan, const char *data);

/*!
	\brief EC control
	*/
void capi_echo_canceller(struct capi_pvt *i, int function);

#ifdef DIVA_STREAMING
struct _diva_streaming_vector;
void capidev_handle_data_b3_indication_vector (struct capi_pvt *i,
																							 struct _diva_streaming_vector* vind,
																							 int vind_nr);
/*!
 * \brief Return true if Diva streaming supported by CAPI controller
 */
int pbx_capi_streaming_supported (struct capi_pvt *i);
#endif

/* DIVA specific MANUFACTURER definitions */
#define _DI_MANU_ID         0x44444944
#define _DI_ASSIGN_PLCI     0x0001
#define _DI_DSP_CTRL        0x0003
#define _DI_OPTIONS_REQUEST 0x0009

#if (!defined(CC_AST_HAS_VERSION_1_4) && !defined(CC_AST_HAS_VERSION_1_6) && !defined(CC_AST_HAS_VERSION_1_8))

#define ast_malloc(__x__)          malloc((__x__))
#define ast_free(__x__)            free((__x__))
#define ast_strdup(__x__)          strdup((__x__))
#define ast_channel_trylock(__x__) ast_mutex_trylock(&(__x__)->lock)
#define ast_channel_unlock(__x__)  ast_mutex_unlock(&(__x__)->lock)
#define ast_devstate_prov_add(__a__,__b__) (-1)
#define ast_devstate_prov_del(__x__) do{}while(0)

#endif

#endif
