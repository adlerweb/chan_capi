#
# An implementation of Common ISDN API 2.0 for Asterisk
#
# Makefile, based on the Asterisk Makefile, Coypright (C) 1999, Mark Spencer
#
# Copyright (C) 2005-2009 Cytronics & Melware
# 
# Armin Schindler <armin@melware.de>
# 
# Reworked, but based on the work of
# Copyright (C) 2002-2005 Junghanns.NET GmbH
#
# Klaus-Peter Junghanns <kapejod@ns1.jnetdns.de>
#
# This program is free software and may be modified and 
# distributed under the terms of the GNU Public License.
#

OSNAME=${shell uname}

DIVA_STREAMING=0
DIVA_STATUS=0
DIVA_VERBOSE=0

USE_OWN_LIBCAPI=yes

.EXPORT_ALL_VARIABLES:

V=0

INSTALL_PREFIX=

ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/usr/include

ifeq (${OSNAME},FreeBSD)
ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/usr/local/include
endif

ifeq (${OSNAME},NetBSD)
ASTERISK_HEADER_DIR=$(INSTALL_PREFIX)/usr/pkg/include
endif

MODULES_DIR=$(INSTALL_PREFIX)/usr/lib/asterisk/modules

ifeq (${OSNAME},FreeBSD)
MODULES_DIR=$(INSTALL_PREFIX)/usr/local/lib/asterisk/modules
endif

ifeq (${OSNAME},NetBSD)
MODULES_DIR=$(INSTALL_PREFIX)/usr/pkg/lib/asterisk/modules
endif

CONFIG_DIR=$(INSTALL_PREFIX)/etc/asterisk

ifeq (${OSNAME},FreeBSD)
CONFIG_DIR=$(INSTALL_PREFIX)/usr/local/etc/asterisk
endif

ifeq (${OSNAME},NetBSD)
CONFIG_DIR=$(INSTALL_PREFIX)/usr/pkg/etc/asterisk
endif

OSARCH=$(shell uname -s)

ifeq ($(strip $(PROC)),)
ifeq (${OSARCH},Linux)
PROC=$(shell uname -m)
else
ifeq (${OSARCH},FreeBSD)
PROC=$(shell uname -m)
endif
endif
endif

AVERSION=$(shell if grep -q "VERSION_NUM 0104" $(ASTERISK_HEADER_DIR)/asterisk/version.h; then echo V1_4; fi)

ifeq (${USE_OWN_LIBCAPI},yes)
INCLUDE= -I./libcapi20 
LIBLINUX= 
else
INCLUDE= 
LIBLINUX=-lcapi20
endif

ifeq (${DIVA_STREAMING},1)
INCLUDE += -I./divastreaming -I./divastreaming/..
endif

ifeq (${DIVA_STATUS},1)
INCLUDE += -I./divastatus -I./divastatus/..
endif

INCLUDE += -I./divaverbose

DEBUG=-g #-pg
INCLUDE+= -I$(ASTERISK_HEADER_DIR)
ifndef C4B
ifeq (${OSNAME},FreeBSD)
INCLUDE+= $(shell [ -d /usr/include/i4b/include ] && \
	echo -n -I/usr/include/i4b/include)
endif
ifeq (${OSNAME},NetBSD)
INCLUDE+= $(shell [ -d /usr/include/i4b/include ] && \
        echo -n -I/usr/include/i4b/include)
endif
endif #C4B

ifdef C4B
LIBLINUX+=-L/usr/local/lib -llinuxcapi20
endif

CFLAGS=-pipe -fPIC -Wall -Wmissing-prototypes -Wmissing-declarations $(DEBUG) $(INCLUDE) -D_REENTRANT -D_GNU_SOURCE
CFLAGS+=$(OPTIMIZE)
CFLAGS+=-Wno-unused-but-set-variable
CFLAGS+=-O2
CFLAGS+=$(shell if $(CC) -march=$(PROC) -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-march=$(PROC)"; fi)
CFLAGS+=$(shell if uname -m | grep -q "ppc\|arm\|s390"; then echo "-fsigned-char"; fi)
ifeq (${DIVA_STREAMING},1)
CFLAGS += -DDIVA_STREAMING=1
endif
ifeq (${DIVA_STATUS},1)
CFLAGS += -DDIVA_STATUS=1

CFLAGS+=$(shell echo '\#include <sys/inotify.h>' > /tmp/test.c 2>/dev/null && \
                echo 'int main(int argc,char**argv){if(inotify_init()>=0)return 0; return 1;}' >> /tmp/test.c 2>/dev/null && \
                $(CC) /tmp/test.c -o /tmp/test && /tmp/test >/dev/null 2>&1 && echo '-DCC_USE_INOTIFY=1'; rm -f /tmp/test.c /tmp/test)
endif
ifeq (${DIVA_VERBOSE},1)
CFLAGS += -DDIVA_VERBOSE=1
endif


LIBS=-ldl -lpthread -lm
CC=gcc
INSTALL=install

SHAREDOS=chan_capi.so

OBJECTS=chan_capi.o chan_capi_utils.o chan_capi_rtp.o chan_capi_command.o xlaw.o dlist.o	\
	chan_capi_qsig_core.o chan_capi_qsig_ecma.o chan_capi_qsig_asn197ade.o	\
	chan_capi_qsig_asn197no.o chan_capi_supplementary.o chan_capi_chat.o \
	chan_capi_mwi.o chan_capi_cli.o chan_capi_ami.o chan_capi_management_common.o \
	chan_capi_devstate.o

ifeq (${USE_OWN_LIBCAPI},yes)
OBJECTS += libcapi20/convert.o libcapi20/capi20.o libcapi20/capifunc.o
endif

ifeq (${DIVA_STREAMING},1)
OBJECTS += divastreaming/diva_streaming_idi_host_ifc_impl.o \
           divastreaming/diva_streaming_idi_host_rx_ifc_impl.o \
           divastreaming/diva_streaming_manager.o \
           divastreaming/diva_streaming_messages.o \
           divastreaming/segment_alloc.o \
           divastreaming/chan_capi_divastreaming_utils.o \
           divastreaming/runtime.o
endif

ifeq (${DIVA_STATUS},1)
OBJECTS += divastatus/divastatus.o
endif

ifeq (${DIVA_VERBOSE},1)
OBJECTS += divaverbose/divaverbose.o
endif

CFLAGS+=-Wno-missing-prototypes -Wno-missing-declarations

CFLAGS+=-DCRYPTO

ifneq (${AVERSION},V1_4)
CFLAGS+=`if grep -q AST_JB config.h; then echo -DAST_JB; fi`
endif

.SUFFIXES: .c .o

all: config.h $(SHAREDOS)

clean:
	rm -f config.h
	rm -f *.so *.o
	rm -f libcapi20/*.o
	rm -f divastreaming/*.o
	rm -f divastatus/*.o
	rm -f divaverbose/*.o

distclean: clean
	rm -f $(MODULES_DIR)/$(SHAREDOS)

sysclean: distclean
	rm -f $(MODULES_DIR)/*.so

config.h:
	./create_config.sh "$(ASTERISK_HEADER_DIR)"

.c.o: config.h
	@if [ "$(V)" = "0" ]; then \
		echo " [CC] $*.c -> $*.o";	\
	else	\
		echo "$(CC) $(CFLAGS) -c $*.c -o $*.o";	\
	fi
	@$(CC) $(CFLAGS) -c $*.c -o $*.o;

$(SHAREDOS): $(OBJECTS)
	@if [ "$(V)" = "0" ]; then \
		echo " [LD] $@ ($^)";	\
	else	\
		echo "$(CC) -shared -Xlinker -x -o $@ $^ $(LIBLINUX)";	\
	fi
	@$(CC) -shared -Xlinker -x -o $@ $^ $(LIBLINUX)

install: all
	$(INSTALL) -d -m 755 $(MODULES_DIR)
	for x in $(SHAREDOS); do $(INSTALL) -m 755 $$x $(MODULES_DIR) ; done

install_config: capi.conf
	$(INSTALL) -d -m 755 ${CONFIG_DIR}
	$(INSTALL) -m 644 capi.conf ${CONFIG_DIR}

samples: install_config

