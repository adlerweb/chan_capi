#ifndef __CAPI20_PLATFORM_H__
#define __CAPI20_PLATFORM_H__

#if __GNUC__ >= 3 /* { */

#ifndef likely
#define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#else /* } { */

#ifndef likely
#define likely(__x__) (!!(__x__))
#endif
#ifndef unlikely
#define unlikely(__x__) (!!(__x__))
#endif

#endif /* } */

#endif
