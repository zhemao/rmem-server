#ifndef _DEBUG_H_
#define _DEBUG_H_

/* errno doesn't define EUNKNOWN, so we do it here.
   This is a dubious hack because errno may not be an int or even an enum.
   In practice it seems to work. */
#define EUNKNOWN -1

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

#define UNIMPLEMENTED \
    printf("%-20s | %3d |  Function unimplemented\n",__FUNCTION__,__LINE__)


#endif // _DEBUG_H_
