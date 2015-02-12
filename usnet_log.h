#ifndef _USNET_LOG_H_
#define _USNET_LOG_H_

#include <stdio.h>
#include <sys/time.h>

//#define DUMP_PAYLOAD

#define NON_DEBUG
#ifdef NON_DEBUG
#define DEBUG(_fmt, ...)     do {} while(0)
#else
#define DEBUG(_fmt, ...)                \
   do {                    \
      struct timeval _t0;           \
      gettimeofday(&_t0, NULL);        \
      fprintf(stdout, "%03d.%06d %s [%d] " _fmt "\n", \
          (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec, \
          __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
        } while (0)
#endif

#define INFO(_fmt, ...)     do {} while(0)
#define WARN(_fmt, ...)     do {} while(0)

#define ERROR(_fmt, ...)     do {} while(0)
/*
#define ERROR(_fmt, ...)                \
   do {                    \
      struct timeval _t0;           \
      gettimeofday(&_t0, NULL);        \
      fprintf(stdout, "%03d.%06d %s [%d] " _fmt "\n", \
          (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec, \
          __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
        } while (0)

*/
#define FATAL(_fmt, ...)     do {} while(0)

#endif //!_USNET_LOG_H_
