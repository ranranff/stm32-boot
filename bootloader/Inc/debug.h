#ifndef __DEBUG_H
#define __DEBUG_H
#include <stdio.h>
#include <assert.h>


#define DEBUG_ON    1
#define WARNING_ON  1
#define ERROR_ON    1

/*-------------------------------------------------------------------------------------------------------------*/
#if DEBUG_ON
#define DEBUG(fmt,...)  do{\
                                  printf("[ debug ] %s:%u %s: ",__FILE__,__LINE__,__FUNCTION__); \
                                  printf(fmt,##__VA_ARGS__); \
                                  printf("\n");}while(0)
#else
#define DEBUG(fmt,...)
#endif

/*-------------------------------------------------------------------------------------------------------------*/
#if DEBUG_ON
#define LOG()  do{\
                                  printf("[ log ] %s:%u %s: ",__FILE__,__LINE__,__FUNCTION__); \
                                  printf("\n");}while(0)
#else
#define LOG()
#endif

/*-------------------------------------------------------------------------------------------------------------*/
#if WARNING_ON
#define DBA_WARNING(fmt,...)  do{\
                                  printf("[ warning ] %s:%u %s: ",__FILE__,__LINE__,__FUNCTION__);\
                                  printf(fmt,##__VA_ARGS__);\
                                  printf("\n");}while(0)
#else
#define DBA_WARNING(fmt,...)
#endif

/*-------------------------------------------------------------------------------------------------------------*/
#if ERROR_ON
#define ERROR(fmt,...)  do{\
                                  printf("[ error ] %s:%u %s: ",__FILE__,__LINE__,__FUNCTION__);\
                                  printf(fmt,##__VA_ARGS__);\
                                  printf("\n");}while(0)
#else
#define ERROR(fmt,...)
#endif

/*-------------------------------------------------------------------------------------------------------------*/
#endif
