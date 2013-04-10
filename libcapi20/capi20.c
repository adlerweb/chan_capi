/*
 * CAPI 2.0 library
 * 
 * 2002-03-27 - Added remote capi features.
 *              Armin Schindler <armin@melware.de>
 *
 * This program is free software and may be modified and 
 * distributed under the terms of the GNU Public License.
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#define _LINUX_LIST_H
#include <linux/capi.h>
 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "capi20_platform.h"
 
#include "capi20.h"

#ifndef CAPI_GET_FLAGS
#define CAPI_GET_FLAGS		_IOR('C',0x23, unsigned)
#endif
#ifndef CAPI_SET_FLAGS
#define CAPI_SET_FLAGS		_IOR('C',0x24, unsigned)
#endif
#ifndef CAPI_CLR_FLAGS
#define CAPI_CLR_FLAGS		_IOR('C',0x25, unsigned)
#endif
#ifndef CAPI_NCCI_OPENCOUNT
#define CAPI_NCCI_OPENCOUNT	_IOR('C',0x26, unsigned)
#endif
#ifndef CAPI_NCCI_GETUNIT
#define CAPI_NCCI_GETUNIT	_IOR('C',0x27, unsigned)
#endif

#define SEND_BUFSIZ		(128+2048)

static char capidevname[] = "/dev/capi20";
static char capidevnamenew[] = "/dev/isdn/capi20";

static int                  capi_fd = -1;
static capi_ioctl_struct    ioctl_data;

static int remote_capi;
static char *globalconfigfilename = "/etc/capi20.conf";
static char *userconfigfilename = ".capi20rc";
static unsigned short int port;
static char hostname[1024];
static int tracelevel;
static char *tracefile;

/* REMOTE-CAPI commands */
 
#define RCAPI_REGISTER_REQ                      CAPICMD(0xf2, 0xff)
#define RCAPI_REGISTER_CONF                     CAPICMD(0xf3, 0xff)
#define RCAPI_GET_MANUFACTURER_REQ              CAPICMD(0xfa, 0xff)
#define RCAPI_GET_MANUFACTURER_CONF             CAPICMD(0xfb, 0xff)
#define RCAPI_GET_SERIAL_NUMBER_REQ             CAPICMD(0xfe, 0xff)
#define RCAPI_GET_SERIAL_NUMBER_CONF            CAPICMD(0xff, 0xff)
#define RCAPI_GET_VERSION_REQ                   CAPICMD(0xfc, 0xff)
#define RCAPI_GET_VERSION_CONF                  CAPICMD(0xfd, 0xff)
#define RCAPI_GET_PROFILE_REQ                   CAPICMD(0xe0, 0xff)
#define RCAPI_GET_PROFILE_CONF                  CAPICMD(0xe1, 0xff)
#define RCAPI_AUTH_USER_REQ                     CAPICMD(0xff, 0x00)
#define RCAPI_AUTH_USER_CONF                    CAPICMD(0xff, 0x01)

static char *skip_whitespace(char *s)
{
	while (*s && isspace(*s)) s++;
		return s;
}

static char *skip_nonwhitespace(char *s)
{
	while (*s && !isspace(*s)) s++;
		return s;
} 

static unsigned char get_byte(unsigned char **p)
{
	*p += 1;
	return((unsigned char)*(*p - 1));
}
 
static unsigned short get_word(unsigned char **p)
{
	return(get_byte(p) | (get_byte(p) << 8));
}

static unsigned short get_netword(unsigned char **p)
{
	return((get_byte(p) << 8) | get_byte(p));
}

#if 0
static unsigned int get_dword(unsigned char **p)
{
	return(get_word(p) | (get_word(p) << 16));
}
#endif
  
static unsigned char *put_byte(unsigned char **p, _cbyte val)
{
	**p = val;
	*p += 1;
	return(*p);
} 

static unsigned char *put_word(unsigned char **p, _cword val)
{
	put_byte(p, val & 0xff);
	put_byte(p, (val & 0xff00) >> 8);
	return(*p);
}

static unsigned char *put_netword(unsigned char **p, _cword val)
{
	put_byte(p, (val & 0xff00) >> 8);
	put_byte(p, val & 0xff);
	return(*p);
}

static unsigned char *put_dword(unsigned char **p, _cdword val)
{
	put_word(p, val & 0xffff);
	put_word(p, (val & 0xffff0000) >> 16);
	return(*p);
} 

/*
 * read config file
 */

static int read_config(void)
{
	FILE *fp = NULL;
	char *s, *t;
	char buf[1024];

	if ((s = getenv("HOME")) != NULL) {
		strcpy(buf, s);
		strcat(buf, "/");
		strcat(buf, userconfigfilename);
		fp = fopen(buf, "r");
	}
	if ((!fp) && ((fp = fopen(globalconfigfilename, "r")) == NULL))
			return(0);

	while(fgets(buf, sizeof(buf), fp)) {
		buf[strlen(buf)-1] = 0;
		s = skip_whitespace(buf);
		if (*s == 0 || *s == '#')
					continue;
		if (!(strncmp(s, "REMOTE", 6))) {
			remote_capi = 1;
			s = skip_nonwhitespace(s);
			
			t = skip_whitespace(s);
			s = skip_nonwhitespace(t);
			if (*s) *s++ = 0;
			strncpy(hostname, t, (sizeof(hostname) - 1));

			t = skip_whitespace(s);
			s = skip_nonwhitespace(t);
			if (*s) *s++ = 0;
			port = strtol(t, NULL, 10);
			if (!port)
					port = 2662;
			continue;
		} else if (!(strncmp(s, "TRACELEVEL", 10))) {
			t = skip_nonwhitespace(s);
			s = skip_whitespace(t);
			tracelevel = (int)strtol(s, NULL, 10);
			continue;
		} else if (!(strncmp(s, "TRACEFILE", 9))) {
			t = skip_nonwhitespace(s);
			s = skip_whitespace(t);
			t = skip_nonwhitespace(s);
			if (*t) *t++ = 0;
			tracefile = strdup(s);
			continue;
		}
	}
	fclose(fp);
	return(1);
}

/*
 *	socket function
 */

static int open_socket(void)
{
	int fd;
	struct hostent *hostinfo;
	struct sockaddr_in sadd;

	/* connect to remote capi */

	fd = socket(PF_INET, SOCK_STREAM, 0);

	sadd.sin_family = AF_INET;
	sadd.sin_port = htons(port);
	hostinfo = gethostbyname(hostname);
	if (hostinfo) {
		sadd.sin_addr = *(struct in_addr *) hostinfo->h_addr;
		if (!connect(fd, (struct sockaddr *) &sadd, sizeof(sadd))) {
			return(fd);
		}	
	}
	
	close(fd);
	return(-1);
}

static int socket_read(int fd, unsigned char *buf, int l)
{
	unsigned char tbuf[4096], *p;
	int tlen, rlen, alen, olen;
	time_t t;

	if(read(fd, tbuf, 2) == 2) {
		p = tbuf;
		tlen = olen = (int)get_netword(&p) - 2;
		t = time(NULL);
		rlen = 0;

		while(((alen = read(fd, &tbuf[rlen], tlen)) < tlen) && (time(NULL) < (t + 5))) {
			if (alen > 0) {
				tlen -= alen;
				rlen += alen;
			}
			alen = 0;
		}
		if (alen > 0)
			rlen += alen;

		if (rlen != olen) {
			return(0);
		}
		if (!l)
			l = olen;
		olen = (l < rlen) ? l : rlen;
		memcpy(buf, tbuf, olen);
		return(olen);
	}
	return(0);
}

static int remote_command(int fd, unsigned char *buf, int len, int conf)
{
	unsigned char *p;
	int l;
		
	if(write(fd, buf, len) < len)
		return(0);

	if ((l = socket_read(fd, buf, 0)) < 1)
		return(0);
			
	p = buf + 4;
	if(get_netword(&p) == conf) {
		memmove(buf, buf + 8, l - 8);
		return(l - 8);
	}
	return(0);
}

static void set_rcapicmd_header(unsigned char **p, int len, _cword cmd, _cdword ctrl)
{
	put_netword(p, len);
	put_word(p, 0);
	put_word(p, 0);
	put_netword(p, cmd);
	put_word(p, 0);
	put_dword(p, ctrl);
}

static void write_capi_trace(int send, unsigned char *buf, int length, int datamsg)
{
	int fd;
	_cdword ltime;
	unsigned char header[7];

	if (!tracefile)
		return;

	if (tracelevel < (datamsg + 1))
		return;

	fd = open(tracefile, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd >= 0) {
		int tmp;

		ltime = (_cdword)time(NULL);
		capimsg_setu16(header, 0, length + sizeof(header));
		capimsg_setu32(header, 2, ltime);
		header[6] = (send) ? 0x80:0x81;
		tmp = write(fd, header, sizeof(header));
		tmp = write(fd, buf, length);
		close(fd);
	}
}

static inline unsigned capi20_isinstalled_internal(void)
{
	if (likely(capi_fd >= 0))
		return CapiNoError;

	return (capi20_isinstalled());
}

unsigned capi20_isinstalled(void)
{
	if (likely(capi_fd >= 0))
		return CapiNoError;

	/*----- open managment link -----*/
	if (read_config() && (remote_capi)) {
		capi_fd = open_socket();
		if (capi_fd >= 0) {
			/* TODO: we could do some AUTH here with rcapid */
			return CapiNoError;
		}
		return CapiRegNotInstalled;
	}

	if ((capi_fd = open(capidevname, O_RDWR, 0666)) < 0 && (errno == ENOENT))
		capi_fd = open(capidevnamenew, O_RDWR, 0666);

	if (capi_fd < 0)
		return CapiRegNotInstalled;

	if (ioctl(capi_fd, CAPI_INSTALLED, 0) == 0)
		return CapiNoError;

	return CapiRegNotInstalled;
}

/*
 * managment of application ids
 */

#define MAX_APPL 1024

static int applidmap[MAX_APPL];

static inline int remember_applid(unsigned applid, int fd)
{
	if (applid >= MAX_APPL)
		return -1;

	applidmap[applid] = fd;
	return 0;
}

static inline unsigned alloc_applid(int fd)
{
	unsigned applid;

	for (applid = 1; applid < MAX_APPL; applid++) {
		if (applidmap[applid] < 0) {
			applidmap[applid] = fd;
			return applid;
		}
	}
	return 0;
}

static inline void freeapplid(unsigned applid)
{
	if (applid < MAX_APPL)
		applidmap[applid] = -1;
}

static inline int validapplid(unsigned applid)
{
	return ((applid > 0) && (applid < MAX_APPL) && (applidmap[applid] >= 0));
}

static inline int applid2fd(unsigned applid)
{
	if (applid < MAX_APPL)
		return applidmap[applid];

	return -1;
}

/*
 * buffer management
 */

struct recvbuffer {
	struct recvbuffer *next;
	unsigned int  datahandle;
	unsigned int  used;
	unsigned int  ncci;
	unsigned char *buf; /* 128 + MaxSizeB3 */
};

struct applinfo {
	unsigned  maxbufs;
	unsigned  nbufs;
	size_t    recvbuffersize;
	struct recvbuffer *buffers;
	struct recvbuffer *firstfree;
	struct recvbuffer *lastfree;
	unsigned char *bufferstart;
};

static struct applinfo *alloc_buffers(
	unsigned MaxB3Connection,
	unsigned MaxB3Blks,
	unsigned MaxSizeB3)
{
	struct applinfo *ap;
	unsigned nbufs = 2 + MaxB3Connection * (MaxB3Blks + 1);
	size_t recvbuffersize = 128 + MaxSizeB3;
	unsigned i;
	size_t size;

	if (recvbuffersize < 2048)
		recvbuffersize = 2048;

	size = sizeof(struct applinfo);
	size += sizeof(struct recvbuffer) * nbufs;
	size += recvbuffersize * nbufs;

	ap = (struct applinfo *)malloc(size);
	if (ap == 0)
		return 0;

	memset(ap, 0, size);
	ap->maxbufs = nbufs;
	ap->recvbuffersize = recvbuffersize;
	ap->buffers = (struct recvbuffer *)(ap+1);
	ap->firstfree = ap->buffers;
	ap->bufferstart = (unsigned char *)(ap->buffers+nbufs);
	for (i = 0; i < ap->maxbufs; i++) {
		ap->buffers[i].next = &ap->buffers[i+1];
		ap->buffers[i].used = 0;
		ap->buffers[i].ncci = 0;
		ap->buffers[i].buf = ap->bufferstart+(recvbuffersize*i);
	}
	ap->lastfree = &ap->buffers[ap->maxbufs-1];
	ap->lastfree->next = 0;
	return ap;
}

static void free_buffers(struct applinfo *ap)
{
	free(ap);
}

static struct applinfo *applinfo[MAX_APPL];

static unsigned char *get_buffer(unsigned applid, size_t *sizep, unsigned *handle)
{
	struct applinfo *ap;
	struct recvbuffer *buf;

	assert(validapplid(applid));
	ap = applinfo[applid];
	if ((buf = ap->firstfree) == 0)
		return 0;

	ap->firstfree = buf->next;
	buf->next = 0;
	buf->used = 1;
	ap->nbufs++;
	*sizep = ap->recvbuffersize;
	*handle  = (buf->buf-ap->bufferstart)/ap->recvbuffersize;

	return buf->buf;
}

static void save_datahandle(
	unsigned char applid,
	unsigned offset,
	unsigned datahandle,
	unsigned ncci)
{
	struct applinfo *ap;
	struct recvbuffer *buf;

	assert(validapplid(applid));
	ap = applinfo[applid];
	assert(offset < ap->maxbufs);
	buf = ap->buffers+offset;
	buf->datahandle = datahandle;
	buf->ncci = ncci;
}

static unsigned return_buffer(unsigned char applid, unsigned offset)
{
	struct applinfo *ap;
	struct recvbuffer *buf;

	assert(validapplid(applid));
	ap = applinfo[applid];
	assert(offset < ap->maxbufs);
	buf = ap->buffers+offset;
	assert(buf->used == 1);
	assert(buf->next == 0);

	if (ap->lastfree) {
		ap->lastfree->next = buf;
		ap->lastfree = buf;
	} else {
		ap->firstfree = ap->lastfree = buf;
	}
	buf->used = 0;
	buf->ncci = 0;
	assert(ap->nbufs-- > 0);

	return buf->datahandle;
}

static void cleanup_buffers_for_ncci(unsigned char applid, unsigned ncci)
{
	struct applinfo *ap;
	unsigned i;
	
	assert(validapplid(applid));
	ap = applinfo[applid];

	for (i = 0; i < ap->maxbufs; i++) {
		if (ap->buffers[i].used) {
			assert(ap->buffers[i].ncci != 0);
			if (ap->buffers[i].ncci == ncci) {
				return_buffer(applid, i);
			}
		}
	}
}

static void cleanup_buffers_for_plci(unsigned char applid, unsigned plci)
{
	struct applinfo *ap;
	unsigned i;
	
	assert(validapplid(applid));
	ap = applinfo[applid];

	for (i = 0; i < ap->maxbufs; i++) {
		if (ap->buffers[i].used) {
			assert(ap->buffers[i].ncci != 0);
			if ((ap->buffers[i].ncci & 0xffff) == plci) {
				return_buffer(applid, i);
			}
		}
	}
}

/* 
 * CAPI2.0 functions
 */

unsigned
capi20_register(
	unsigned MaxB3Connection,
	unsigned MaxB3Blks,
	unsigned MaxSizeB3,
	unsigned *ApplID)
{
	int applid = 0;
	char buf[PATH_MAX];
	int i, fd = -1;

    *ApplID = 0;

    if (capi20_isinstalled() != CapiNoError)
       return CapiRegNotInstalled;
	if ((!remote_capi) || ((remote_capi) && ((fd = open_socket()) < 0))) {
	    if ((fd = open(capidevname, O_RDWR|O_NONBLOCK, 0666)) < 0 && 
		     (errno == ENOENT)) {
			fd = open(capidevnamenew, O_RDWR|O_NONBLOCK, 0666);
		}
	}

    if (fd < 0) {
		return CapiRegOSResourceErr;
	}

    ioctl_data.rparams.level3cnt = MaxB3Connection;
    ioctl_data.rparams.datablkcnt = MaxB3Blks;
    ioctl_data.rparams.datablklen = MaxSizeB3;

	if (remote_capi) {
		unsigned char buf[100];
		unsigned char *p = buf;
		int errcode;
		set_rcapicmd_header(&p, 23, RCAPI_REGISTER_REQ, 0);
		put_word(&p, 2048); /* fake size */
		put_word(&p, MaxB3Connection);
		put_word(&p, MaxB3Blks);
		put_word(&p, MaxSizeB3);
		put_byte(&p, 2); /* capi version */
		if(!(remote_command(fd, buf, 23, RCAPI_REGISTER_CONF))) {
			close(fd);
			return CapiMsgOSResourceErr;
		}
		p = buf;
		errcode = get_word(&p);
		if(errcode == CapiNoError) {
			applid = alloc_applid(fd);
		} else {
			close(fd);
			return(errcode);
		}
	} else if ((applid = ioctl(fd, CAPI_REGISTER, &ioctl_data)) < 0) {
		if (errno == EIO) {
			if (ioctl(fd, CAPI_GET_ERRCODE, &ioctl_data) < 0) {
				close (fd);
				return CapiRegOSResourceErr;
			}
			close (fd);
			return (unsigned)ioctl_data.errcode;
		} else if (errno == EINVAL) { // old kernel driver
			close (fd);
			fd = -1;
			for (i = 0; fd < 0; i++) {
				/*----- open pseudo-clone device -----*/
				sprintf(buf, "/dev/capi20.%02d", i);
				if ((fd = open(buf, O_RDWR|O_NONBLOCK, 0666)) < 0) {
					switch (errno) {
					case EEXIST:
						break;
					default:
						return CapiRegOSResourceErr;
					}
				}
			}
			if (fd < 0)
				return CapiRegOSResourceErr;

			ioctl_data.rparams.level3cnt = MaxB3Connection;
			ioctl_data.rparams.datablkcnt = MaxB3Blks;
			ioctl_data.rparams.datablklen = MaxSizeB3;

			if ((applid = ioctl(fd, CAPI_REGISTER, &ioctl_data)) < 0) {
				if (errno == EIO) {
					if (ioctl(fd, CAPI_GET_ERRCODE, &ioctl_data) < 0) {
						close(fd);
						return CapiRegOSResourceErr;
					}
					close(fd);
					return (unsigned)ioctl_data.errcode;
				}
				close(fd);
				return CapiRegOSResourceErr;
			}
		    applid = alloc_applid(fd);
		} // end old driver compatibility
	}
	if (remember_applid(applid, fd) < 0) {
		close(fd);
		return CapiRegOSResourceErr;
	}
	applinfo[applid] = alloc_buffers(MaxB3Connection, MaxB3Blks, MaxSizeB3);
	if (applinfo[applid] == 0) {
		close(fd);
		return CapiRegOSResourceErr;
	}
	*ApplID = applid;
	return CapiNoError;
}

unsigned
capi20_release (unsigned ApplID)
{
	if (capi20_isinstalled() != CapiNoError)
		return CapiRegNotInstalled;

	if (!validapplid(ApplID))
		return CapiIllAppNr;

	(void)close(applid2fd(ApplID));
	freeapplid(ApplID);
	free_buffers(applinfo[ApplID]);
	applinfo[ApplID] = 0;

	return CapiNoError;
}

unsigned
capi20_put_message (unsigned ApplID, unsigned char *Msg)
{
	unsigned char sndbuf[SEND_BUFSIZ], *sbuf;
	unsigned ret;
	int len = (Msg[0] | (Msg[1] << 8));
	int cmd = Msg[4];
	int subcmd = Msg[5];
	int rc;
	int fd;
	int datareq = 0;

	if (capi20_isinstalled_internal() != CapiNoError)
		return CapiRegNotInstalled;

	if (unlikely(!validapplid(ApplID)))
		return CapiIllAppNr;

	fd = applid2fd(ApplID);

	sbuf = sndbuf;
	if (remote_capi) {
		sbuf = sndbuf + 2;
	}

	memcpy(sbuf, Msg, len);

	if (cmd == CAPI_DATA_B3) {
		datareq = 1;
		if (subcmd == CAPI_REQ) {
			int datalen = (Msg[16] | (Msg[17] << 8));
			void *dataptr;
			if (sizeof(void *) != 4) {
				if (len >= 30) { /* 64Bit CAPI-extention */
					u_int64_t data64;
					memcpy(&data64,Msg+22, sizeof(u_int64_t));
					if (data64 != 0) {
						dataptr = (void *)(unsigned long)data64;
					} else {
						dataptr = Msg + len; /* Assume data after message */
					}
				} else {
					dataptr = Msg + len; /* Assume data after message */
				}
			} else {
				u_int32_t data = (Msg[12] | (Msg[13] << 8) | (Msg[14] << 16) | (Msg[15] << 24));
				if (data != 0) {
					dataptr = (void *)(unsigned long)data;
				} else {
					dataptr = Msg + len; /* Assume data after message */
				}
			}
			if (len + datalen > SEND_BUFSIZ)
				return CapiMsgOSResourceErr;
			memcpy(sbuf+len, dataptr, datalen);
			len += datalen;
		} else if (subcmd == CAPI_RESP) {
			capimsg_setu16(sbuf, 12,
			return_buffer(ApplID, CAPIMSG_U16(sbuf, 12)));
		}
	}

	if (cmd == CAPI_DISCONNECT_B3 && subcmd == CAPI_RESP)
		cleanup_buffers_for_ncci(ApplID, CAPIMSG_U32(sbuf, 8));   

	ret = CapiNoError;
	errno = 0;

	write_capi_trace(1, sbuf, len, datareq);

	if (remote_capi) {
		len += 2;
		sbuf = sndbuf;
		put_netword(&sbuf, len);
	}
    if ((rc = write(fd, sndbuf, len)) != len) {
		if (remote_capi) {
			ret = CapiMsgOSResourceErr;
		} else {
        	switch (errno) {
	            case EFAULT:
    	        case EINVAL:
	                ret = CapiIllCmdOrSubcmdOrMsgToSmall;
	                break;
	            case EBADF:
	                ret = CapiIllAppNr;
	                break;
	            case EIO:
	                if (ioctl(fd, CAPI_GET_ERRCODE, &ioctl_data) < 0) {
	                    ret = CapiMsgOSResourceErr;
	                } else {
						ret = (unsigned)ioctl_data.errcode;
					}
	                break;
	          default:
	                ret = CapiMsgOSResourceErr;
	                break;
	       }
	    }
	}

    return ret;
}

unsigned
capi20_get_message (unsigned ApplID, unsigned char **Buf)
{
	unsigned char *rcvbuf;
	unsigned offset;
	unsigned ret;
	size_t bufsiz;
	int rc, fd;

	if (capi20_isinstalled_internal() != CapiNoError)
		return CapiRegNotInstalled;

	if (unlikely(!validapplid(ApplID)))
		return CapiIllAppNr;

	fd = applid2fd(ApplID);

	if ((*Buf = rcvbuf = get_buffer(ApplID, &bufsiz, &offset)) == 0)
		return CapiMsgOSResourceErr;

	if (remote_capi) {
		rc = socket_read(fd, rcvbuf, bufsiz);
	} else {
		rc = read(fd, rcvbuf, bufsiz);
	}

	if (rc > 0) {
		write_capi_trace(0, rcvbuf, rc, (CAPIMSG_COMMAND(rcvbuf) == CAPI_DATA_B3)? 1:0);
		CAPIMSG_SETAPPID(rcvbuf, ApplID); // workaround for old driver
		if ((CAPIMSG_COMMAND(rcvbuf) == CAPI_DATA_B3) &&
		    (CAPIMSG_SUBCOMMAND(rcvbuf) == CAPI_IND)) {
			save_datahandle(ApplID, offset, CAPIMSG_U16(rcvbuf, 18),
				CAPIMSG_U32(rcvbuf, 8));
			capimsg_setu16(rcvbuf, 18, offset); /* patch datahandle */
			if (sizeof(void *) == 4) {
				u_int32_t data = (u_int32_t)(unsigned long)rcvbuf + CAPIMSG_LEN(rcvbuf);
				rcvbuf[12] = data & 0xff;
				rcvbuf[13] = (data >> 8) & 0xff;
				rcvbuf[14] = (data >> 16) & 0xff;
				rcvbuf[15] = (data >> 24) & 0xff;
			} else {
				u_int64_t data;
				ulong radr = (ulong)rcvbuf;
				if (CAPIMSG_LEN(rcvbuf) < 30) {
					/*
					 * grr, 64bit arch, but no data64 included,
					 * seems to be old driver
					 */
					memmove(rcvbuf+30, rcvbuf+CAPIMSG_LEN(rcvbuf),
						CAPIMSG_DATALEN(rcvbuf));
					rcvbuf[0] = 30;
					rcvbuf[1] = 0;
				}
				data = radr + CAPIMSG_LEN(rcvbuf);
				rcvbuf[12] = rcvbuf[13] = rcvbuf[14] = rcvbuf[15] = 0;
				rcvbuf[22] = data & 0xff;
				rcvbuf[23] = (data >> 8) & 0xff;
				rcvbuf[24] = (data >> 16) & 0xff;
				rcvbuf[25] = (data >> 24) & 0xff;
				rcvbuf[26] = (data >> 32) & 0xff;
				rcvbuf[27] = (data >> 40) & 0xff;
				rcvbuf[28] = (data >> 48) & 0xff;
				rcvbuf[29] = (data >> 56) & 0xff;
			}
			/* keep buffer */
			return CapiNoError;
		}
		return_buffer(ApplID, offset);
		if ((CAPIMSG_COMMAND(rcvbuf) == CAPI_DISCONNECT) &&
		    (CAPIMSG_SUBCOMMAND(rcvbuf) == CAPI_IND)) {
			cleanup_buffers_for_plci(ApplID, CAPIMSG_U32(rcvbuf, 8));
		}
		return CapiNoError;
	}

	return_buffer(ApplID, offset);

	if (rc == 0)
		return CapiReceiveQueueEmpty;

	switch (errno) {
	case EMSGSIZE:
		ret = CapiIllCmdOrSubcmdOrMsgToSmall;
		break;
	case EAGAIN:
		return CapiReceiveQueueEmpty;
	default:
		ret = CapiMsgOSResourceErr;
		break;
	}

	return ret;
}

unsigned char *
capi20_get_manufacturer(unsigned Ctrl, unsigned char *Buf)
{
	if (capi20_isinstalled() != CapiNoError)
		return 0;

	if (remote_capi) {
		unsigned char buf[100];
		unsigned char *p = buf;
		set_rcapicmd_header(&p, 14, RCAPI_GET_MANUFACTURER_REQ, Ctrl);
		if (!(remote_command(capi_fd, buf, 14, RCAPI_GET_MANUFACTURER_CONF)))
			return 0;
		memcpy(Buf, buf + 1, CAPI_MANUFACTURER_LEN);
		Buf[CAPI_MANUFACTURER_LEN-1] = 0;
		return Buf;
	}

    ioctl_data.contr = Ctrl;

	if (ioctl(capi_fd, CAPI_GET_MANUFACTURER, &ioctl_data) < 0)
		return 0;

	memcpy(Buf, ioctl_data.manufacturer, CAPI_MANUFACTURER_LEN);
	Buf[CAPI_MANUFACTURER_LEN-1] = 0;

	return Buf;
}

unsigned char *
capi20_get_version(unsigned Ctrl, unsigned char *Buf)
{
	if (capi20_isinstalled() != CapiNoError)
		return 0;

	if (remote_capi) {
		unsigned char buf[100];
		unsigned char *p = buf;
		set_rcapicmd_header(&p, 14, RCAPI_GET_VERSION_REQ, Ctrl);
		if(!(remote_command(capi_fd, buf, 14, RCAPI_GET_VERSION_CONF)))
			return 0;
		memcpy(Buf, buf + 1, sizeof(capi_version));
		return Buf;
	}

	ioctl_data.contr = Ctrl;
	if (ioctl(capi_fd, CAPI_GET_VERSION, &ioctl_data) < 0) {
		return 0;
	}
	memcpy(Buf, &ioctl_data.version, sizeof(capi_version));
	return Buf;
}

unsigned char * 
capi20_get_serial_number(unsigned Ctrl, unsigned char *Buf)
{
	if (capi20_isinstalled() != CapiNoError)
		return 0;

	if (remote_capi) {
		unsigned char buf[100];
		unsigned char *p = buf;
		set_rcapicmd_header(&p, 14, RCAPI_GET_SERIAL_NUMBER_REQ, Ctrl);
		if(!(remote_command(capi_fd, buf, 14, RCAPI_GET_SERIAL_NUMBER_CONF)))
			return 0;
		memcpy(Buf, buf + 1, CAPI_SERIAL_LEN);
		Buf[CAPI_SERIAL_LEN-1] = 0;
		return Buf;
	}

	ioctl_data.contr = Ctrl;

	if (ioctl(capi_fd, CAPI_GET_SERIAL, &ioctl_data) < 0)
		return 0;

	memcpy(Buf, &ioctl_data.serial, CAPI_SERIAL_LEN);
	Buf[CAPI_SERIAL_LEN-1] = 0;

	return Buf;
}

unsigned
capi20_get_profile(unsigned Ctrl, unsigned char *Buf)
{
	if (capi20_isinstalled() != CapiNoError)
		return CapiMsgNotInstalled;

	if (remote_capi) {
		unsigned char buf[100];
		unsigned char *p = buf;
		unsigned fret;

		set_rcapicmd_header(&p, 14, RCAPI_GET_PROFILE_REQ, Ctrl);
		if(!(remote_command(capi_fd, buf, 14, RCAPI_GET_PROFILE_CONF)))
			return CapiMsgOSResourceErr;

		{
			unsigned short* tmp = (unsigned short*)buf;

			if(*tmp == CapiNoError) {
				memcpy(Buf, buf + 2, (Ctrl) ? sizeof(struct capi_profile) : 2);
			}

			fret = *tmp; 
		}

		return (fret);
	}

	ioctl_data.contr = Ctrl;

	if (ioctl(capi_fd, CAPI_GET_PROFILE, &ioctl_data) < 0) {
		if (errno != EIO)
			return CapiMsgOSResourceErr;
		if (ioctl(capi_fd, CAPI_GET_ERRCODE, &ioctl_data) < 0)
			return CapiMsgOSResourceErr;
		return (unsigned)ioctl_data.errcode;
	}
	if (Ctrl) {
		memcpy(Buf, &ioctl_data.profile, sizeof(struct capi_profile));
	} else {
		memcpy(Buf, &ioctl_data.profile.ncontroller,
			sizeof(ioctl_data.profile.ncontroller));
	}
	return CapiNoError;
}
/*
 * functions added to the CAPI2.0 spec
 */

unsigned
capi20_waitformessage(unsigned ApplID, struct timeval *TimeOut)
{
	int fd;
	fd_set rfds;

	FD_ZERO(&rfds);

	if (capi20_isinstalled_internal() != CapiNoError)
		return CapiRegNotInstalled;

	if (unlikely(!validapplid(ApplID)))
		return CapiIllAppNr;
  
	fd = applid2fd(ApplID);

	FD_SET(fd, &rfds);
  
	if (select(fd + 1, &rfds, NULL, NULL, TimeOut) < 1)
		return CapiReceiveQueueEmpty;
  
	return CapiNoError;
}

int
capi20_fileno(unsigned ApplID)
{
	return applid2fd(ApplID);
}

/*
 * Extensions for middleware
 */

int
capi20ext_get_flags(unsigned ApplID, unsigned *flagsptr)
{
	if (remote_capi)
		return CapiMsgOSResourceErr;

	if (ioctl(applid2fd(ApplID), CAPI_GET_FLAGS, flagsptr) < 0)
		return CapiMsgOSResourceErr;

	return CapiNoError;
}

int
capi20ext_set_flags(unsigned ApplID, unsigned flags)
{
	if (remote_capi)
		return CapiMsgOSResourceErr;

	if (ioctl(applid2fd(ApplID), CAPI_SET_FLAGS, &flags) < 0)
		return CapiMsgOSResourceErr;

	return CapiNoError;
}

int
capi20ext_clr_flags(unsigned ApplID, unsigned flags)
{
	if (remote_capi)
		return CapiMsgOSResourceErr;

	if (ioctl(applid2fd(ApplID), CAPI_CLR_FLAGS, &flags) < 0)
		return CapiMsgOSResourceErr;

	return CapiNoError;
}

char *
capi20ext_get_tty_devname(unsigned applid, unsigned ncci, char *buf, size_t size)
{
	int unit;

	if (remote_capi)
		return NULL;

	unit = ioctl(applid2fd(applid), CAPI_NCCI_GETUNIT, &ncci);
	if (unit < 0)
		return NULL;

	snprintf(buf, size, "/dev/capi/%d", unit);
	return buf;
}

char *
capi20ext_get_raw_devname(unsigned applid, unsigned ncci, char *buf, size_t size)
{
	int unit;

	if (remote_capi)
		return NULL;

	unit = ioctl(applid2fd(applid), CAPI_NCCI_GETUNIT, &ncci);
	if (unit < 0)
		return NULL;

	snprintf(buf, size, "/dev/capi/r%d", unit);
	return buf;
}

int capi20ext_ncci_opencount(unsigned applid, unsigned ncci)
{
	if (remote_capi)
		return CapiMsgOSResourceErr;

	return ioctl(applid2fd(applid), CAPI_NCCI_OPENCOUNT, &ncci);
}

static void initlib(void) __attribute__((constructor));
static void exitlib(void) __attribute__((destructor));

static void initlib(void)
{
	int i;

	for (i = 0; i < MAX_APPL; i++) {
		applidmap[i] = -1;
	}
}

static void exitlib(void)
{
	remote_capi = 0;

    if (capi_fd >= 0) {
       close(capi_fd);
       capi_fd = -1;
    }
}

