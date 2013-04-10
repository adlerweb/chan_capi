/*
 * The "capi20.h" header file is common to all 
 * CAPI 2.0 implementations, and must be included 
 * first. Else the checks below will fail.
 */

#include <capi20.h>

#undef CAPI_OS_HINT

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || \
     defined(__NetBSD__)  || defined(__APPLE__))

#if (CAPI_STACK_VERSION < 204)
/*
 * This looks like CAPI 2.0 for active ISDN cards,
 * CAPI4BSD, and not CAPI for passive ISDN cards, 
 * ISDN4BSD!
 */
#include <capi_bsd.h>
#include <capiutils.h>
#define CAPI_OS_HINT 1
#else /* (CAPI_STACK_VERSION < 204) */
#define CAPI_OS_HINT 2
#endif /* (CAPI_STACK_VERSION < 204) */

#else /* BSD */
#include <linux/capi.h>
#include <capiutils.h>
#endif /* BSD */

#ifndef HEADER_CID
#define HEADER_CID(x) ((x)->adr.adrNCCI)
#endif

#ifndef HEADER_CMD

/* 
 * The following macros have their 
 * origin in ISDN4BSD 1.5.5+
 */

enum
{
    CAPI_PACKED = 0x84  /* non-standard */
};

/*
 * Table is sorted by highest usage first:
 */

#define CAPI_COMMANDS(m,n) \
/*m(n, enum                         , value )*  \
 *m(n,------------------------------,-------)*/ \
  m(n, DATA_B3                      , 0x0086)   \
  m(n, CONNECT                      , 0x0002)   \
  m(n, CONNECT_ACTIVE               , 0x0003)   \
  m(n, CONNECT_B3                   , 0x0082)   \
  m(n, CONNECT_B3_ACTIVE            , 0x0083)   \
  m(n, CONNECT_B3_T90_ACTIVE        , 0x0088)   \
  m(n, DISCONNECT                   , 0x0004)   \
  m(n, DISCONNECT_B3                , 0x0084)   \
  m(n, ALERT                        , 0x0001)   \
  m(n, INFO                         , 0x0008)   \
  m(n, SELECT_B_PROTOCOL            , 0x0041)   \
  m(n, FACILITY                     , 0x0080)   \
  m(n, RESET_B3                     , 0x0087)   \
  m(n, MANUFACTURER                 , 0x00FF)   \
  m(n, LISTEN                       , 0x0005)   \

/* packed version */
#define CAPI_P_REQ(cmd)   ((CAPI_##cmd##_INDEX << 1)^(CAPI_PACKED << 8)^0x7e)
#define CAPI_P_CONF(cmd)  ((CAPI_##cmd##_INDEX << 1)^(CAPI_PACKED << 8)^0x81)
#define CAPI_P_IND(cmd)   ((CAPI_##cmd##_INDEX << 1)^(CAPI_PACKED << 8)^0x80)
#define CAPI_P_RESP(cmd)  ((CAPI_##cmd##_INDEX << 1)^(CAPI_PACKED << 8)^0x7f)

#define CAPI_P_MIN (((CAPI_COMMAND_INDEX_MAX-1) << 1)^(CAPI_PACKED << 8)^0x7e) /* inclusive */
#define CAPI_P_MAX (    (CAPI_COMMAND_INDEX_MAX << 1)^(CAPI_PACKED << 8)^0x80) /* exclusive */

#define CAPI_MAKE_DEF_2(n,ENUM,value)           \
  CAPI_##ENUM##_INDEX,

enum
{
    CAPI_COMMANDS(CAPI_MAKE_DEF_2,)
    CAPI_COMMAND_INDEX_MAX /* exclusive */
};

#define __CAPI_COMMAND_PACK(n, ENUM, value)     \
  ((n) == (value)) ? ((CAPI_##ENUM##_INDEX) << 1) :

/* this macro takes any command and outputs a
 * packed CAPI command (which is better suited
 * for switching)
 *
 * define this as a function so that the
 * compiler does not expand it, hence
 * it is pretty large
 */
#undef CAPI_COMMAND_PACK
static __inline u_int16_t CAPI_COMMAND_PACK(u_int16_t cmd)
{
  return 
    ((((cmd) >> 8) == CAPI_PACKED) ? (cmd) :
     ((((cmd) >> 8) == CAPI_REQ)  ? ((CAPI_PACKED << 8)^0x7e) :
      (((cmd) >> 8) == CAPI_CONF) ? ((CAPI_PACKED << 8)^0x81) :
      (((cmd) >> 8) == CAPI_IND)  ? ((CAPI_PACKED << 8)^0x80) :
      (((cmd) >> 8) == CAPI_RESP) ? ((CAPI_PACKED << 8)^0x7f) :
      0) ^
     (CAPI_COMMANDS(__CAPI_COMMAND_PACK, (cmd) & 0xFF) 0x7e));
}

#define HEADER_CMD(x) \
  CAPI_COMMAND_PACK((((x)->Subcommand) << 8)|(x)->Command)

#endif /* HEADER_CMD */

#ifndef HEADER_MSGNUM
#define HEADER_MSGNUM(x) ((x)->Messagenumber)
#endif

#ifndef FACILITY_RESP_FACILITYRESPONSEPARAMETERS
#define FACILITY_RESP_FACILITYRESPONSEPARAMETERS(x) \
  ((x)->FacilityResponseParameters)
#endif

