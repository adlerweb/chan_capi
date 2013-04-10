#ifndef CAPI_XLAW_H
#define CAPI_XLAW_H

#define capi_int2ulaw(x) capiINT2ULAW[((unsigned short)x) >> 2]
#define capi_int2alaw(x) capiINT2ALAW[(x>>4)+4096]

extern const unsigned char capi_reversebits[256];
extern const short capiULAW2INT[];
extern const unsigned char capiINT2ULAW[16384];
extern const short capiALAW2INT[];
extern const unsigned char capiINT2ALAW[8192];

#endif

