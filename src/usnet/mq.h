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
 * @(#)mq.h
 */

#ifndef _USNET_MQ_H_
#define _USNET_MQ_H_ 

#include "shm.h"

static const int E_DEQUEUE_BUF_NOT_ENOUGH = -13001;
/*
typedef struct tagMQStat
{
	unsigned _used_len;
	unsigned _free_len;
	unsigned _total_len;
	unsigned _shm_key;
	unsigned _shm_id;
	unsigned _shm_size;
   unsigned _enqueued_msg_cnt;
   unsigned _dequeued_msg_cnt;
}TMQStat;

void get_stat(TMQStat& mq_stat)
{
  
   unsigned head = *_head;
   unsigned tail = *_tail;
		
   mq_stat._used_len = (tail>=head) ? tail-head : tail+_block_size-head;
   mq_stat._free_len = head>tail? head-tail: head+_block_size-tail;
   mq_stat._total_len = _block_size;
   mq_stat._shm_key = _shm->key();
   mq_stat._shm_id = _shm->id();
   mq_stat._shm_size = _shm->size();
   
   mq_stat._enqueued_msg_cnt = *_enqueued_msg_cnt;
   mq_stat._dequeued_msg_cnt = *_dequeued_msg_cnt;
}
typedef struct tagFifoSyncMQStat
{
	CSemLockMQ::TSemLockMQStat _semlockmq_stat;
	unsigned _wait_sec;
	unsigned _wait_usec;
}TFifoSyncMQStat;


typedef struct tagSemLockMQStat
{
	TMQStat _mq_stat;
	unsigned _sem_key;
	unsigned _sem_id;
	unsigned _sem_index;
	unsigned _sem_size;
}TSemLockMQStat;

void get_stat(TSemLockMQStat& mq_stat)
{
	_mq.get_stat(mq_stat._mq_stat);
	mq_stat._sem_key = _sem->key();
	mq_stat._sem_id = _sem->id();
	mq_stat._sem_size = _sem->size();
	mq_stat._sem_index = _sem_index;
}

void get_stat(TFifoSyncMQStat& mq_stat)
{
   //_mq.get_stat(mq_stat._semlockmq_stat);
   //mq_stat._wait_sec = _wait_sec;
   //mq_stat._wait_usec = _wait_usec;
}
*/

typedef struct usn_adapctl usn_adapctl_t;
struct usn_adapctl 
{
   unsigned int  m_uiCheckTimeSpan;
   unsigned int  m_uiMsgCount;
   unsigned int  m_uiLastMsgCount;
   unsigned int  m_uiFactor;
   unsigned int  m_uiLastFactor;
   time_t        m_uiLastCheckTime;
   unsigned int  m_uiSync;
}__attribute__((packed));

typedef struct usn_semlockmq usn_semlockmq_t;
struct usn_semlockmq
{
	// TODO: to be defined.
};

typedef struct usn_shmmq usn_shmmq_t;
struct usn_shmmq
{
	usn_shm_t* _shm;
   //usn_semlock_t* _sem; //lock for multi-threaded apps
   uint32_t        _fd;          //fifo file, used for notification.
   uint32_t        _wait_sec;
   uint32_t        _wait_usec;
   uint32_t        _count;
   usn_adapctl_t *_adaptive_ctrl;

	uint32_t*       _head;
	uint32_t*       _tail;
	char*           _block;
	uint32_t        _block_size;
   uint32_t*       _enqueued_msg_cnt;
   uint32_t*       _dequeued_msg_cnt;
#define C_HEAD_SIZE 8
}__attribute__((packed));

void 
print_shmmq(usn_shmmq_t *mq);

int 
usnet_init_shmmq(usn_shmmq_t *mq, char* fifo_path, 
      int32_t wait_sec, int32_t wait_usec, 
      int32_t shm_key, int32_t shm_size, int32_t sync);

void 
usnet_release_shmmq(usn_shmmq_t *mq);

int 
usnet_shmmq_enqueue(usn_shmmq_t *mq, 
      const time_t uiCurTime, const void* data, 
      uint32_t data_len, uint32_t flow);

int 
usnet_shmmq_dequeue(usn_shmmq_t *mq, void* buf, 
      uint32_t buf_size, int32_t *data_len, uint32_t *flow);

int 
usnet_shmmq_dequeue_wait(usn_shmmq_t *mq, void* buf, 
      uint32_t buf_size, int32_t *data_len, uint32_t *flow);

void 
usnet_shmmq_clear_flag(uint32_t _fd);

	
#endif//_USNET_MQ_H_

