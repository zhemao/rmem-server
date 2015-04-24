#ifndef _LOG_H_
#define _LOG_H_

#define DEBUG

#include <stdio.h>

#define LOG_LEVEL 10

#define LOG(LEVEL, STR) do {\
    if ((LEVEL) < LOG_LEVEL) {\
        printf("%-20s | %3d |  ",__FUNCTION__,__LINE__); \
        printf STR; \
    } \
} while(0);

#endif // _LOG_H_
