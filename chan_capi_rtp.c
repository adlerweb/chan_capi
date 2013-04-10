/*
 * An implementation of Common ISDN API 2.0 for Asterisk
 *
 * Copyright (C) 2006-2009 Cytronics & Melware
 *
 * Armin Schindler <armin@melware.de>
 * 
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "chan_capi_platform.h"
#include "chan_capi20.h"
#include "chan_capi.h"
#include "chan_capi_rtp.h"
#include "chan_capi_utils.h"

/* RTP settings / NCPI RTP struct */

static unsigned char NCPI_voice_over_ip_alaw[] =
/* Len Options          */
  "\x27\x00\x00\x00\x00"
/* Len Filt */
  "\x00"
/* Len Tem PT  Seq     Timestamp       SSRC             */
  "\x0c\x80\x00\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78"
/* Len Ulaw    Alaw     */
  "\x04\x00\x00\x08\x08"
/* Len Alaw     */
  "\x02\x08\x08"
/* Len UlawLen Opts    IntervalAlawLen Opts    Interval */
  "\x0c\x00\x04\x03\x00\xa0\x00\x08\x04\x03\x00\xa0\x00";

static unsigned char NCPI_voice_over_ip_ulaw[] =
/* Len Options          */
  "\x27\x00\x00\x00\x00"
/* Len Filt */
  "\x00"
/* Len Tem PT  Seq     Timestamp       SSRC             */
  "\x0c\x80\x00\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78"
/* Len Ulaw    Alaw     */
  "\x04\x00\x00\x08\x08"
/* Len Ulaw     */
  "\x02\x00\x00"
/* Len UlawLen Opts    IntervalAlawLen Opts    Interval */
  "\x0c\x00\x04\x03\x00\xa0\x00\x08\x04\x03\x00\xa0\x00";

static unsigned char NCPI_voice_over_ip_gsm[] =
/* Len Options          */
  "\x27\x00\x00\x00\x00"
/* Len Filt */
  "\x00"
/* Len Tem PT  Seq     Timestamp       SSRC             */
  "\x0c\x80\x00\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78"
/* Len GSM     Alaw     */
  "\x04\x03\x03\x08\x08"
/* Len GSM      */
  "\x02\x03\x03"
/* Len GSM Len Opts    IntervalAlawLen Opts    Interval */
  "\x0c\x03\x04\x0f\x00\xa0\x00\x08\x04\x00\x00\xa0\x00";

static unsigned char NCPI_voice_over_ip_g723[] =
/* Len Options          */
  "\x27\x00\x00\x00\x00"
/* Len Filt */
  "\x00"
/* Len Tem PT  Seq     Timestamp       SSRC             */
  "\x0c\x80\x00\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78"
/* Len G723    Alaw     */
  "\x04\x04\x04\x08\x08"
/* Len G723     */
  "\x02\x04\x04"
/* Len G723Len Opts    IntervalAlawLen Opts    Interval */
  "\x0c\x04\x04\x01\x00\xa0\x00\x08\x04\x00\x00\xa0\x00";

static unsigned char NCPI_voice_over_ip_g726[] =
/* Len Options          */
  "\x27\x00\x00\x00\x00"
/* Len Filt */
  "\x00"
/* Len Tem PT  Seq     Timestamp       SSRC             */
  "\x0c\x80\x00\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78"
/* Len G726    Alaw     */
  "\x04\x02\x02\x08\x08"
/* Len G726     */
  "\x02\x02\x02"
/* Len G726Len Opts    IntervalAlawLen Opts    Interval */
  "\x0c\x02\x04\x0f\x00\xa0\x00\x08\x04\x00\x00\xa0\x00";

static unsigned char NCPI_voice_over_ip_g729[] =
/* Len Options          */
  "\x27\x00\x00\x00\x00"
/* Len Filt */
  "\x00"
/* Len Tem PT  Seq     Timestamp       SSRC             */
  "\x0c\x80\x00\x00\x00\x00\x00\x00\x00\x12\x34\x56\x78"
/* Len G729    Alaw     */
  "\x04\x12\x12\x08\x08"
/* Len G729     */
  "\x02\x12\x12"
/* Len G729Len Opts    IntervalAlawLen Opts    Interval */
  "\x0c\x12\x04\x0f\x00\xa0\x00\x08\x04\x00\x00\xa0\x00";


/*
 * return NCPI for chosen RTP codec
 */
_cstruct capi_rtp_ncpi(struct capi_pvt *i)
{
	_cstruct ncpi = NULL;

	if ((i) && (i->owner) &&
	    (i->bproto == CC_BPROTO_RTP)) {
		switch(i->codec) {
		case CC_FORMAT_ALAW:
			ncpi = NCPI_voice_over_ip_alaw;
			break;
		case CC_FORMAT_ULAW:
			ncpi = NCPI_voice_over_ip_ulaw;
			break;
		case CC_FORMAT_GSM:
			ncpi = NCPI_voice_over_ip_gsm;
			break;
		case CC_FORMAT_G723_1:
			ncpi = NCPI_voice_over_ip_g723;
			break;
		case CC_FORMAT_G726:
			ncpi = NCPI_voice_over_ip_g726;
			break;
		case CC_FORMAT_G729A:
			ncpi = NCPI_voice_over_ip_g729;
			break;
		default:
			cc_log(LOG_ERROR, "%s: format %s(%d) invalid.\n",
				i->vname, cc_getformatname(i->codec), i->codec);
			break;
		}
	}

	return ncpi;
}

/*
 * create rtp for capi interface
 */
int capi_alloc_rtp(struct capi_pvt *i)
{
#ifndef CC_AST_HAS_VERSION_10_0 /* Use vocoder without RTP framing */
#ifdef CC_AST_HAS_AST_SOCKADDR
	struct ast_sockaddr addr;
	struct ast_sockaddr us;
#else
	struct ast_hostent ahp;
	struct hostent *hp;
	struct in_addr addr;
	struct sockaddr_in us;
#endif
#ifndef CC_AST_HAS_VERSION_1_4
	char temp[MAXHOSTNAMELEN];
#endif

#ifdef CC_AST_HAS_AST_SOCKADDR
	ast_sockaddr_parse(&addr, "localhost:0", 0);
#else
	hp = ast_gethostbyname("localhost", &ahp);
	memcpy(&addr, hp->h_addr, sizeof(addr));
#endif

#ifdef CC_AST_HAS_RTP_ENGINE_H
#ifdef CC_AST_HAS_AST_SOCKADDR
	i->rtp = ast_rtp_instance_new(NULL, NULL, &addr, NULL);
#else
	i->rtp = ast_rtp_instance_new(NULL, NULL, (struct sockaddr_in *)&addr, NULL);
#endif
#else
	i->rtp = ast_rtp_new_with_bindaddr(NULL, NULL, 0, 0, addr);
#endif

	if (!(i->rtp)) {
		cc_log(LOG_ERROR, "%s: unable to alloc rtp.\n", i->vname);
		return 1;
	}
#ifdef CC_AST_HAS_RTP_ENGINE_H
	ast_rtp_instance_get_local_address(i->rtp, &us);
	ast_rtp_instance_set_remote_address(i->rtp, &us);
#else
	ast_rtp_get_us(i->rtp, &us);
	ast_rtp_set_peer(i->rtp, &us);
#endif
	cc_verbose(2, 1, VERBOSE_PREFIX_4 "%s: alloc rtp socket on %s:%d\n",
		i->vname,
#ifdef CC_AST_HAS_AST_SOCKADDR
		ast_sockaddr_stringify(&us), ntohs(ast_sockaddr_port(&us)));
#else
#ifdef CC_AST_HAS_VERSION_1_4
		ast_inet_ntoa(us.sin_addr),
#else
		ast_inet_ntoa(temp, sizeof(temp), us.sin_addr),
#endif
		ntohs(us.sin_port));
#endif
	i->timestamp = 0;
	return 0;
#else
	i->rtp = 0;
	cc_log(LOG_ERROR, "%s: use vocoder\n", i->vname);
	return 1;
#endif
}

/*
 * write rtp for a channel
 */
int capi_write_rtp(struct capi_pvt *i, struct ast_frame *f)
{
#ifndef CC_AST_HAS_VERSION_10_0 /* Use vocoder */
#ifdef CC_AST_HAS_AST_SOCKADDR
	struct ast_sockaddr us;
#else
	struct sockaddr_in us;
	socklen_t uslen = sizeof(us);
#endif
	int len;
	unsigned int *rtpheader;
	unsigned char buf[256];

	if (!(i->rtp)) {
		cc_log(LOG_ERROR, "rtp struct is NULL\n");
		return -1;
	}

#ifdef CC_AST_HAS_RTP_ENGINE_H
	ast_rtp_instance_get_local_address(i->rtp, &us);
	ast_rtp_instance_set_remote_address(i->rtp, &us);
	if (ast_rtp_instance_write(i->rtp, f) != 0) {
#else
	ast_rtp_get_us(i->rtp, &us);
	ast_rtp_set_peer(i->rtp, &us);
	if (ast_rtp_write(i->rtp, f) != 0) {
#endif
		cc_verbose(3, 0, VERBOSE_PREFIX_2 "%s: rtp_write error, dropping packet.\n",
			i->vname);
		return 0;
	}

	while(1) {
#ifdef CC_AST_HAS_AST_SOCKADDR
		len = ast_recvfrom(ast_rtp_instance_fd(i->rtp, 0), buf, sizeof(buf), 0, &us);
#else
#ifdef CC_AST_HAS_RTP_ENGINE_H
		len = recvfrom(ast_rtp_instance_fd(i->rtp, 0),
			buf, sizeof(buf), 0, (struct sockaddr *)&us, &uslen);
#else
		len = recvfrom(ast_rtp_fd(i->rtp),
			buf, sizeof(buf), 0, (struct sockaddr *)&us, &uslen);
#endif
#endif
		if (len <= 0)
			break;

		rtpheader = (unsigned int *)buf;
		
		rtpheader[1] = htonl(i->timestamp);
		i->timestamp += CAPI_MAX_B3_BLOCK_SIZE;
			
		if (len > (CAPI_MAX_B3_BLOCK_SIZE + RTP_HEADER_SIZE)) {
			cc_verbose(4, 0, VERBOSE_PREFIX_4 "%s: rtp write data: frame too big (len = %d).\n",
				i->vname, len);
			continue;
		}
		if (i->B3count >= CAPI_MAX_B3_BLOCKS) {
			cc_verbose(3, 1, VERBOSE_PREFIX_4 "%s: B3count is full, dropping packet.\n",
				i->vname);
			continue;
		}
		cc_mutex_lock(&i->lock);
		i->B3count++;
		cc_mutex_unlock(&i->lock);

		i->send_buffer_handle++;

		cc_verbose(6, 1, VERBOSE_PREFIX_4 "%s: RTP write for NCCI=%#x len=%d(%d) %s ts=%x\n",
			i->vname, i->NCCI, len, f->datalen, cc_getformatname(GET_FRAME_SUBCLASS_CODEC(f->subclass)),
			i->timestamp);

		capi_sendf(NULL, 0, CAPI_DATA_B3_REQ, i->NCCI, get_capi_MessageNumber(),
			"dwww",
			buf,
			len,
			i->send_buffer_handle,
			0
		);
	}

#endif

	return 0;
}

/*
 * read data b3 in RTP mode
 */
struct ast_frame *capi_read_rtp(struct capi_pvt *i, unsigned char *buf, int len)
{
#ifndef CC_AST_HAS_VERSION_10_0 /* Use vocoder */
	struct ast_frame *f;
#ifdef CC_AST_HAS_AST_SOCKADDR
	struct ast_sockaddr us;
#else
	struct sockaddr_in us;
#endif

	if (!(i->owner))
		return NULL;

	if (!(i->rtp)) {
		cc_log(LOG_ERROR, "rtp struct is NULL\n");
		return NULL;
	}

#ifdef CC_AST_HAS_RTP_ENGINE_H
	ast_rtp_instance_get_local_address(i->rtp, &us);
	ast_rtp_instance_set_remote_address(i->rtp, &us);
#else
	ast_rtp_get_us(i->rtp, &us);
	ast_rtp_set_peer(i->rtp, &us);
#endif

#ifdef CC_AST_HAS_AST_SOCKADDR
	if (len != ast_sendto(ast_rtp_instance_fd(i->rtp, 0), buf, len, 0, &us))
#else
#ifdef CC_AST_HAS_RTP_ENGINE_H
	if (len != sendto(ast_rtp_instance_fd(i->rtp, 0), buf, len, 0, (struct sockaddr *)&us, sizeof(us)))
#else
	if (len != sendto(ast_rtp_fd(i->rtp), buf, len, 0, (struct sockaddr *)&us, sizeof(us)))
#endif
#endif
	{
		cc_verbose(4, 1, VERBOSE_PREFIX_3 "%s: RTP sendto error\n",
			i->vname);
		return NULL;
	}

#ifdef CC_AST_HAS_RTP_ENGINE_H
	if ((f = ast_rtp_instance_read(i->rtp, 0))) {
#else
	if ((f = ast_rtp_read(i->rtp))) {
#endif
		if (f->frametype != AST_FRAME_VOICE) {
			cc_verbose(3, 1, VERBOSE_PREFIX_3 "%s: DATA_B3_IND RTP (len=%d) non voice type=%d\n",
				i->vname, len, f->frametype);
			return NULL;
		}
		cc_verbose(6, 1, VERBOSE_PREFIX_4 "%s: DATA_B3_IND RTP NCCI=%#x len=%d %s (read/write=%d/%d)\n",
			i->vname, i->NCCI, len, cc_getformatname(GET_FRAME_SUBCLASS_CODEC(f->subclass)),
			i->owner->readformat, i->owner->writeformat);
		if (i->owner->nativeformats != GET_FRAME_SUBCLASS_CODEC(f->subclass)) {
			cc_verbose(3, 1, VERBOSE_PREFIX_3 "%s: DATA_B3_IND RTP nativeformats=%d, but subclass=%ld\n",
				i->vname, i->owner->nativeformats, GET_FRAME_SUBCLASS_CODEC(f->subclass));
			i->owner->nativeformats = GET_FRAME_SUBCLASS_CODEC(f->subclass);
			ast_set_read_format(i->owner, i->owner->readformat);
			ast_set_write_format(i->owner, i->owner->writeformat);
		}
	}
	return f;
#else
	return NULL;
#endif
}

/*
 * eval RTP profile
 */
void voice_over_ip_profile(struct cc_capi_controller *cp)
{
	MESSAGE_EXCHANGE_ERROR error;
	_cmsg CMSG;
	struct timeval tv;
	unsigned char fac[4] = "\x03\x02\x00\x00";
	int waitcount = 200;
	unsigned short info = 0;
	unsigned int payload1, payload2;

	capi_sendf(NULL, 0, CAPI_FACILITY_REQ, cp->controller, get_capi_MessageNumber(),
		"ws",
		FACILITYSELECTOR_VOICE_OVER_IP,
		&fac
	);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	
	while (waitcount) {
		error = capi20_waitformessage(capi_ApplID, &tv);
		error = capi_get_cmsg(&CMSG, capi_ApplID); 
		if (error == 0) {
			if (IS_FACILITY_CONF(&CMSG)) {
				info = 1;
				break;
			}
		}
		usleep(20000);
		waitcount--;
	} 
	if (!info) {
		cc_log(LOG_WARNING, "did not receive FACILITY_CONF\n");
		return;
	}

	/* parse profile */
	if (FACILITY_CONF_FACILITYSELECTOR(&CMSG) != FACILITYSELECTOR_VOICE_OVER_IP) {
		cc_log(LOG_WARNING, "unexpected FACILITY_SELECTOR = %#x\n",
			FACILITY_CONF_FACILITYSELECTOR(&CMSG));
		return;
	}
	if ((info = FACILITY_CONF_INFO(&CMSG)) != 0x0000) {
		cc_verbose(3, 0, VERBOSE_PREFIX_4 "FACILITY_CONF INFO = %#x, RTP not used.\n",
			info);
		return;

	}
	if (FACILITY_CONF_FACILITYCONFIRMATIONPARAMETER(&CMSG)[0] < 13) {
		cc_log(LOG_WARNING, "conf parameter too short %d, RTP not used.\n",
			FACILITY_CONF_FACILITYCONFIRMATIONPARAMETER(&CMSG)[0]);
		return;
	}
	info = read_capi_word(&(FACILITY_CONF_FACILITYCONFIRMATIONPARAMETER(&CMSG)[1]));
	if (info != 0x0002) {
		cc_verbose(3, 0, VERBOSE_PREFIX_4 "FACILITY_CONF wrong parameter (0x%04x), RTP not used.\n",
			info);
		return;
	}
	info = read_capi_word(&(FACILITY_CONF_FACILITYCONFIRMATIONPARAMETER(&CMSG)[4]));
	payload1 = read_capi_dword(&(FACILITY_CONF_FACILITYCONFIRMATIONPARAMETER(&CMSG)[6]));
	payload2 = read_capi_dword(&(FACILITY_CONF_FACILITYCONFIRMATIONPARAMETER(&CMSG)[10]));
	cc_verbose(3, 0, VERBOSE_PREFIX_4 "RTP payload options 0x%04x 0x%08x 0x%08x\n",
		info, payload1, payload2);

	cc_verbose(3, 0, VERBOSE_PREFIX_4 "RTP codec: ");
	if (payload1 & 0x00000100) {
		cp->rtpcodec |= CC_FORMAT_ALAW;
		cc_verbose(3, 0, "G.711-alaw ");
	}
	if (payload1 & 0x00000001) {
		cp->rtpcodec |= CC_FORMAT_ULAW;
		cc_verbose(3, 0, "G.711-ulaw ");
	}
	if (payload1 & 0x00000008) {
		cp->rtpcodec |= CC_FORMAT_GSM;
		cc_verbose(3, 0, "GSM ");
	}
	if (payload1 & 0x00000010) {
		cp->rtpcodec |= CC_FORMAT_G723_1;
		cc_verbose(3, 0, "G.723.1 ");
	}
	if (payload1 & 0x00000004) {
		cp->rtpcodec |= CC_FORMAT_G726;
		cc_verbose(3, 0, "G.726 ");
	}
	if (payload1 & 0x00040000) {
		cp->rtpcodec |= CC_FORMAT_G729A;
		cc_verbose(3, 0, "G.729 ");
	}
	if (payload1 & (1U << 27)) {
		cp->rtpcodec |= CC_FORMAT_ILBC;
		cc_verbose(3, 0, "iLBC ");
	}
#ifdef CC_FORMAT_G722
	if (payload1 & (1U << 9)) {
		cp->rtpcodec |= CC_FORMAT_G722;
		cc_verbose(3, 0, "G.722 ");
	}
#endif
#if defined(CC_FORMAT_SIREN7) && defined(CC_FORMAT_SIREN14)
	if (payload1 & (1U << 24)) {
#ifdef CC_FORMAT_SIREN7
		cp->rtpcodec |= CC_FORMAT_SIREN7;
		cc_verbose(3, 0, "siren7 ");
#endif
#ifdef CC_FORMAT_SIREN14
		cp->rtpcodec |= CC_FORMAT_SIREN14;
		cc_verbose(3, 0, "siren14 ");
#endif
	}
#endif
#if defined(CC_FORMAT_SLINEAR) || defined(CC_FORMAT_SLINEAR16)
	if (payload1 & (1U << 1)) {
#if defined(CC_FORMAT_SLINEAR)
		cp->rtpcodec |= CC_FORMAT_SLINEAR;
		cc_verbose(3, 0, "slin ");
#endif
#if defined(CC_FORMAT_SLINEAR16)
		cp->rtpcodec |= CC_FORMAT_SLINEAR16;
		cc_verbose(3, 0, "slin16 ");
#endif
	}
#endif

	cc_verbose(3, 0, "\n");
}

