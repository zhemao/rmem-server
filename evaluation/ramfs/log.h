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

#define LOG(file, STR) do {\
    fprintf(file, "%-20s | %3d |  ",__FUNCTION__,__LINE__); \
    fprintf(file, STR); \
} while(0);

#endif // _LOG_H_
