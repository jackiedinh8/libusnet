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
 * @(#)log.c
 */

#define _WITH_DPRINTF
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <pthread_np.h>

#include "log.h"

usn_log_t *g_global_log;

usn_log_t*
usnet_log_init(const char* filename, int level, uint32_t rotate_num, long size_limit)
{
    //uint32_t position;
    usn_log_t *log;

    log = (usn_log_t*) malloc(sizeof(log));
    if ( log == NULL )
       return NULL;
    memset(log, 0, sizeof(log));

    // other settings
    log->_level = level;
    log->_rotate_num = rotate_num;
    log->_size_limit = size_limit;
    log->_log_file_name = filename;
   
    log->_fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, 0666);

    if ( log->_fd < 0 ) {
       printf("cannot open file, %s\n", filename);
       perror("error");
    }
    /*
    position = filename.find_last_of("/");
    _path_dir = filename.substr(0, position + 1);
    _log_file_name = filename.substr(position + 1);
    _pure_name = _log_file_name;
    _pure_name.erase(_pure_name.length() - 4, 4);
    init_list_file();
    */

    return log;
}

void init_list_file(usn_log_t *log) {
   return;
   /*
    struct dirent *ent;
    _dir = opendir(_path_dir.c_str());
    if (_dir != NULL) {
        while ((ent = readdir(_dir)) != NULL) {
            std::string file_name = ent->d_name;
            //     std::cout << "file name = " << file_name << "\n";
            size_t name_pos = file_name.find(_pure_name);
            //   std::cout << "name position = " << name_pos << "\n";
            if (name_pos != std::string::npos) {
                name_pos += _pure_name.size();
                //     std::cout << "name position after move= " << name_pos << "\n";
                size_t tail_pos = file_name.rfind(".log");
                //      std::cout << "dot position = " << tail_pos << "\n";
                if (tail_pos != std::string::npos) {
                    if (tail_pos - name_pos > 0) {
                        int number = atoi(file_name.substr(name_pos, tail_pos - name_pos).c_str());
                        //       std::cout << "number = " << number << "\n";
                        _list_file.push_front(number);
                    } else if (tail_pos - name_pos == 0) {
                        _list_file.push_front(0);
                    }
                }
            }
        }
    }

    _list_file.sort();
    _list_file.reverse();

    closedir(_dir);
    if (_list_file.empty()) {
        //   std::cout << "no log file here -> create new\n";
        open_log_file();
        _list_file.push_front(0);
    } else {
        //  std::cout << "list begin = " << _list_file.front() << "\n";
        //    std::cout << "list back = " << _list_file.back() << "\n";
        rotate_file();
        open_log_file();
    }
    */
}

void usnet_log(usn_log_t *log, int level, const char* sourcefilename, int line, const char* msg, ...) {
    static char* level_str[] = 
        {"INFO", "DEBUG", "WARN", "ERROR", "FATAL"};

    if ( log == NULL )
       return;

    if ( log->_fd == -1 )
       return;

    if (level >= log->_level) {
        //todo: check file limit size, if exceeds, do rotation.
        //long log_file_size = _file.tellp();

        /*
        if (log_file_size >= _size_limit) {
            rotate_file();
            open_log_file();
        }
        */
        char dest[1024] = {0};
        va_list argptr;
        va_start(argptr, msg);
        vsprintf(dest, msg, argptr);
        va_end(argptr);
        //check level, if higher, then log the msg ( set time of msg arrival, human readable).

        time_t curTime = time(0);
        char* date = asctime(localtime(&curTime));
        date[strlen(date) - 1] = 0;
        dprintf(log->_fd, "[%d]%s:%d[%s]: %s\n", pthread_getthreadid_np(), sourcefilename, line, level_str[level], dest);

        //_file << sourcefilename << ":" <<  line << "[" << level_str[level] << "]:"  << dest << std::endl;
    }
}

void open_log_file(usn_log_t *log) {
   /*
    if (!_file.is_open())
        _file.open((_path_dir + _log_file_name).c_str());
        */
}

void rotate_file(usn_log_t *log) {
   /*
    if (_file.is_open())
        _file.close();

    if (_list_file.size() >= _rotate_num) {
        //remove latest file
        char str[10];
        sprintf(str, "%d", _list_file.front());
        std::string num = str;
        remove((_path_dir + _pure_name + num + ".log").c_str());
        _list_file.pop_front();
        //      std::cout << "remove file: "<<num<<"\n";
    }
    for (std::list<int>::iterator iter = _list_file.begin(); iter != _list_file.end(); iter++) {
        char str[10];
        sprintf(str, "%d", (*iter));
        std::string old_num = ((*iter) == 0 ? "" : str);

        (*iter)++; //tang len 1

        char str2[10];
        sprintf(str2, "%d", (*iter));
        std::string new_num = str2;

        rename((_path_dir + _pure_name + old_num + ".log").c_str(), (_path_dir + _pure_name + new_num + ".log").c_str());
        //    std::cout << "rename file: "<<old_num << " to " << new_num<<"\n";
    }

    //push new file to list
    _list_file.push_back(0);
    */
}


