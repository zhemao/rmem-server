#ifndef _LOG_H_
#define _LOG_H_

#define DEBUG

#include <stdio.h>

#ifndef LOG_LEVEL
#ifdef DEBUG
#define LOG_LEVEL 10
#else // !DEBUG
#define LOG_LEVEL 1
#endif // DEBUG
#endif // LOG_LEVEL


#define LOG(level, STR) do {\
    if ((level) < LOG_LEVEL) {\
        printf("%-20s | %3d |  ",__FUNCTION__,__LINE__); \
        printf STR; \
       fflush(stdout);\
       fflush(stderr);\
    }\
} while(0);

#endif // _LOG_H_
