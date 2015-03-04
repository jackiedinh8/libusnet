#ifndef _USNET_MQ_H_
#define _USNET_MQ_H_ 

#include "usnet_shm.h"

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
   u_int32        _fd;          //fifo file, used for notification.
   u_int32        _wait_sec;
   u_int32        _wait_usec;
   u_int32        _count;
   usn_adapctl_t *_adaptive_ctrl;

	u_int32*       _head;
	u_int32*       _tail;
	char*          _block;
	u_int32        _block_size;
   u_int32*       _enqueued_msg_cnt;
   u_int32*       _dequeued_msg_cnt;
#define C_HEAD_SIZE 8
}__attribute__((packed));

void 
print_shmmq(usn_shmmq_t *mq);

int32 usnet_init_shmmq(usn_shmmq_t *mq, char* fifo_path, 
      u_int32 wait_sec, u_int32 wait_usec, 
      int32 shm_key, u_int32 shm_size);

void 
usnet_release_shmmq(usn_shmmq_t *mq);

int32 
usnet_shmmq_enqueue(usn_shmmq_t *mq, const time_t uiCurTime, 
      const void* data, u_int32 data_len, u_int32 flow);

int32 
usnet_shmmq_dequeue(usn_shmmq_t *mq, void* buf, 
      u_int32 buf_size, u_int32 *data_len, u_int32 *flow);

void usnet_shmmq_clear_flag();
	
#endif//_USNET_MQ_H_

