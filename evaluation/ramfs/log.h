#ifndef _LOG_H_
#define _LOG_H_

#define DEBUG

#include <stdio.h>

#define LOG_LEVEL 10

#define LOG(level, STR) do {\
    if ((level) < LOG_LEVEL) {\
        printf("%-20s | %3d |  ",__FUNCTION__,__LINE__); \
        printf STR; \
       fflush(stdout);\
       fflush(stderr);\
    }\
} while(0);

#endif // _LOG_H_
