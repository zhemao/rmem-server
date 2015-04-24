#ifndef _DEBUG_H_
#define _DEBUG_H_

#define CHECK_ERROR(cond, STR) do {\
    if (cond) {\
        printf("%-20s | %3d |  ",__FUNCTION__,__LINE__); \
        printf STR; \
        exit(EXIT_FAILURE);\
    } \
} while(0);

#define RETURN_ERROR(cond, return_value, STR) do {\
    if (cond) {\
        printf("%-20s | %3d |  ",__FUNCTION__,__LINE__); \
        printf STR; \
        return return_value; \
    } \
} while(0);

#endif // _DEBUG_H_
