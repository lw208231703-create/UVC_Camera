#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#ifdef _WIN32

// Windows: timeval may already be defined by winsock2.h or winsock.h
// (both included via windows.h → libusb, etc.).  Guard on both the
// standard _TIMEVAL_DEFINED macro and any check for the struct itself.
#ifndef _TIMEVAL_DEFINED
#ifndef _WINSOCK2API_
#ifndef _WINSOCKAPI_
#include <time.h>
struct timeval {
    long tv_sec;
    long tv_usec;
};
#define _TIMEVAL_DEFINED
#endif /* _WINSOCKAPI_ */
#endif /* _WINSOCK2API_ */
#endif /* _TIMEVAL_DEFINED */

#else
#include_next <sys/time.h>
#endif

#endif /* _SYS_TIME_H */
