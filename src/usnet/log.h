/*
 * Copyright (c) 2015 Jackie Dinh <jackiedinh8@gmail.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  1 Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  2 Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution.
 *  3 Neither the name of the <organization> nor the 
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @(#)log.h
 */

#ifndef _USNET_LOG_H_
#define _USNET_LOG_H_

#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>

#include "types.h"

enum {           
	USN_INFO = 0,
   USN_DEBUG = 1,
   USN_WARN = 2,
   USN_ERROR = 3,
   USN_FATAL = 4
};

struct usn_log {
    int             _level;
    uint32_t        _rotate_num;
    long            _size_limit;
    const char*     _log_file_name;
    int             _fd;
    /*
    std::ofstream _file;
    DIR* _dir;
    std::string _log_file_name;
    std::string _path_dir;
    std::string _pure_name;
    
    std::list<int> _list_file;
    */
    
};

usn_log_t*
usnet_log_init(const char* filename, int level, uint32_t rotate_num, long size_limit);

void 
usnet_log(usn_log_t *log, int level, const char* sourcefilename, int line, const char* msg, ...);

void init_list_file();
void open_log_file();
void rotate_file();

extern usn_log_t *g_global_log;

// make use of__FILE__, __LINE__
#define LOG(logger,level,fmt, ...) \
{  usnet_log(logger,level,__FILE__, __LINE__,fmt, ##__VA_ARGS__); }

//#define DUMP_PAYLOAD

#define TRACE(log,_fmt, ...)     do {} while(0)

//#define DEBUG(log,_fmt, ...)  do {LOG(log, USN_DEBUG,_fmt,##__VA_ARGS__);} while(0)
#define DEBUG(log,_fmt, ...)     do {} while(0)

#define INFO(log,_fmt, ...)     do {} while(0)

#define WARN(log,_fmt, ...)  do {LOG(log, USN_WARN,_fmt,##__VA_ARGS__);} while(0)

#define ERROR(log,_fmt, ...)  do {LOG(log, USN_ERROR,_fmt,##__VA_ARGS__);} while(0)
//#define ERROR(log,_fmt, ...)     do {} while(0)

/*
#define ERROR(_fmt, ...)                \
   do {                    \
      struct timeval _t0;           \
      gettimeofday(&_t0, NULL);        \
      fprintf(stdout, "%03d.%06d %s [ERROR] [%d] " _fmt "\n", \
          (int)(_t0.tv_sec % 1000), (int)_t0.tv_usec, \
          __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
        } while (0)
*/

#define FATAL(_fmt, ...)     do {} while(0)


#endif // _USNET_LOG_H_
