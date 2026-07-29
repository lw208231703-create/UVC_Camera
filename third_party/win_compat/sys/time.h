#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#ifdef _WIN32

#include <time.h>

#ifndef _TIMEVAL_DEFINED
#ifndef _WINSOCK2API_
#ifndef _WINSOCKAPI_
struct timeval {
    long tv_sec;
    long tv_usec;
};
#define _TIMEVAL_DEFINED
#define _WINSOCKAPI_
#endif
#endif
#endif

#else
#include_next <sys/time.h>
#endif

#endif
