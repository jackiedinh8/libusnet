#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "usnet_mq.h"

void print_shmmq(usn_shmmq_t *mq) 
{
   printf("shmmq info, shm_key=%lu, shm_size=%lu, fd=%d, wait_sec=%d, wait_usec=%d "
          "mem=%p, _enqueued_msg_cnt=%p, _dequeued_msg_cnt=%p, head=%p, tail=%p, block=%p, block_size=%d \n", 
         mq->_shm->key, mq->_shm->size,
         mq->_fd, mq->_wait_sec, mq->_wait_usec,
         mq->_shm->mem, mq->_enqueued_msg_cnt, mq->_dequeued_msg_cnt,
         mq->_head, mq->_tail, mq->_block, mq->_block_size
         );
   printf("head=%d, tail=%d, enqueued_msg_cnt=%d, dequeued_msg_cnt=%d\n",
           *mq->_head, *mq->_tail,
           *mq->_enqueued_msg_cnt, *mq->_dequeued_msg_cnt);
   return;
}

void print_shmmq_stat(usn_shmmq_t *mq) 
{

}
int 
usnet_init_adapctl(usn_adapctl_t *ctl, 
      const time_t uiCurTime, 
      const unsigned int uiCheckTimeSpan)
{
    ctl->m_uiCheckTimeSpan = uiCheckTimeSpan;
    ctl->m_uiMsgCount = 0;
    ctl->m_uiLastCheckTime = uiCurTime;
    ctl->m_uiFactor = 1;
    return 0;
}

int GenNotifyFactor(const unsigned int uiMsgCount)
{
    if (uiMsgCount <= 1000)
    {
        return 1;
    }
    else if (uiMsgCount <= 5000)
    {
        return 4;
    }
    else if (uiMsgCount <= 10000)
    {
        return 5;
    }
    else if (uiMsgCount <= 20000)
    {
        return 6;
    }
    else if (uiMsgCount <= 30000)
    {
        return 9;
    }
    else if (uiMsgCount <= 40000)
    {
        return 15;
    }
    else if (uiMsgCount <= 50000)
    {
        return 24;
    }
    else if (uiMsgCount <= 60000)
    {
        return 39;
    }
    else if (uiMsgCount <= 70000)
    {
        return 63;
    }
    else if (uiMsgCount <= 80000)
    {
        return 73;
    }
    else if (uiMsgCount <= 90000)
    {
        return 83;
    }
    else if (uiMsgCount <= 100000)
    {
        return 93;
    }
    return 100;

}
int 
usnet_adapctl_addload(usn_adapctl_t *ctl,
      const time_t uiCurTime, 
      const unsigned int uiMsgCount)
{
    if (uiCurTime < ctl->m_uiLastCheckTime + (int)ctl->m_uiCheckTimeSpan)
    {
        ctl->m_uiMsgCount = ctl->m_uiMsgCount + uiMsgCount;
    }
    else
    {
        ctl->m_uiLastCheckTime = uiCurTime;
        ctl->m_uiLastMsgCount = ctl->m_uiMsgCount;
        ctl->m_uiMsgCount = uiMsgCount;
        ctl->m_uiLastFactor = ctl->m_uiFactor;
        ctl->m_uiFactor = GenNotifyFactor(ctl->m_uiLastMsgCount);
    }  
    return 0;
}


int usnet_init_shmmq(usn_shmmq_t *shmmq, char* fifo_path, 
      unsigned wait_sec, unsigned wait_usec, 
      int shm_key, unsigned shm_size)
{
   int ret = 0;
   int val;
   char *mem_addr = NULL;
   int   mode = 0666 | O_NONBLOCK | O_NDELAY;

   if ( shmmq == NULL ) 
      return -1;

   
	errno = 0;
    if ((mkfifo(fifo_path, mode)) < 0)
		if (errno != EEXIST) {
			ret = -1;
         goto done;
      }

   if ((shmmq->_fd = open(fifo_path, O_RDWR)) < 0) {
		ret = -2;
      goto done;
   }
	if (shmmq->_fd > 1024)
	{
		close(shmmq->_fd);
		ret = -3;
      goto done;
	}
    
	val = fcntl(shmmq->_fd, F_GETFL, 0);
	
	if (val == -1) {
		ret = errno ? -errno : val;
      goto done;
   }
	
	if (val & O_NONBLOCK) {
		ret = 0;
      goto done;
   }
	
	ret = fcntl(shmmq->_fd, F_SETFL, val | O_NONBLOCK | O_NDELAY);

	if (ret < 0) {
      ret = errno ? -errno : ret;
      goto done;
   } else
      ret = 0;

	assert(shm_size > C_HEAD_SIZE);
	shmmq->_shm = usnet_shm_create(shm_key, shm_size);
   if ( shmmq->_shm == NULL ) {
      printf("open only\n");
      shmmq->_shm = usnet_shm_open(shm_key, shm_size);
      if ( shmmq->_shm == NULL ) {
		   ret = -1;
         goto done;
   	}
      mem_addr = shmmq->_shm->mem;
      printf("goto setup\n");
      goto setup;
	} else
      mem_addr = shmmq->_shm->mem;

   // init head portion of shared meme.
   memset(mem_addr, 0, C_HEAD_SIZE * 2 + sizeof(*shmmq->_adaptive_ctrl));

   // init adaptive control.
   shmmq->_adaptive_ctrl = (usn_adapctl_t *)mem_addr;
   shmmq->_adaptive_ctrl->m_uiCheckTimeSpan = 3;
   shmmq->_adaptive_ctrl->m_uiMsgCount = 0;
   shmmq->_adaptive_ctrl->m_uiLastCheckTime = time(NULL);
   shmmq->_adaptive_ctrl->m_uiLastFactor = 1;
   shmmq->_adaptive_ctrl->m_uiFactor = 1;

	shmmq->_wait_sec = wait_sec;
	shmmq->_wait_usec = wait_usec;
 
setup:
   shmmq->_adaptive_ctrl = (usn_adapctl_t *)mem_addr;
   mem_addr += sizeof(*shmmq->_adaptive_ctrl);
   shmmq->_enqueued_msg_cnt = (unsigned*)mem_addr;
   shmmq->_dequeued_msg_cnt = (unsigned*)mem_addr + 1;

   //	set head and tail
	shmmq->_head = (unsigned*)mem_addr + 2;
	shmmq->_tail = shmmq->_head+1;
	shmmq->_block = (char*) (shmmq->_tail+1);
	shmmq->_block_size = shm_size - ( C_HEAD_SIZE * 2 + sizeof(*shmmq->_adaptive_ctrl) );

	ret = 0;
done:
   return ret;
}

void 
usnet_release_shmmq(usn_shmmq_t *mq)
{
   // TODO
}

int usnet_write_mq(usn_shmmq_t *mq, const void* data, unsigned data_len, unsigned flow)
{
	unsigned head;// = *_head;
	unsigned tail;// = *_tail;	
	unsigned free_len;// = head>tail? head-tail: head+_block_size-tail;
	unsigned tail_len;// = _block_size - tail;
	char sHead[C_HEAD_SIZE] = {0};
	unsigned total_len;// = data_len+C_HEAD_SIZE;
   int ret = 0;

   if ( mq == NULL )
      return -1;

   head = *mq->_head;
   tail = *mq->_tail;
	free_len = head>tail? head-tail : head + mq->_block_size - tail;
	tail_len = mq->_block_size - tail;
	total_len = data_len+C_HEAD_SIZE;

   // get lock
	//_sem->wait(mq->_sem_index);

	//	first, if no enough space?
	if (free_len <= total_len)
	{
		//LOG(FATAL, "CShmMQ::enqueue: No more free shm mem, 
      //           free_len:%d, total_len:%d", free_len, total_len);
		ret = -1;
      goto done;
	}

	memcpy(sHead, &total_len, sizeof(unsigned));
	memcpy(sHead+sizeof(unsigned), &flow, sizeof(unsigned));

	//	second, if tail space > 8+len
	//	copy 8 byte, copy data
	if (tail_len >= total_len)
	{
		memcpy(mq->_block+tail, sHead, C_HEAD_SIZE);
		memcpy(mq->_block+tail+ C_HEAD_SIZE, data, data_len);
		*mq->_tail += data_len + C_HEAD_SIZE;
	}

	//	third, if tail space > 8 && < 8+len
	else if (tail_len >= C_HEAD_SIZE && tail_len < C_HEAD_SIZE+data_len)
	{
		//	copy 8 byte
		memcpy(mq->_block+tail, sHead, C_HEAD_SIZE);

		//	copy tail-8
		unsigned first_len = tail_len - C_HEAD_SIZE;
		memcpy(mq->_block+tail+ C_HEAD_SIZE, data, first_len);

		//	copy left
		unsigned second_len = data_len - first_len;
		memcpy(mq->_block, ((char*)data) + first_len, second_len);

        // XXX
		int itmp = *mq->_tail + data_len + C_HEAD_SIZE - mq->_block_size;
		*mq->_tail = itmp;

		//*_tail += data_len + C_HEAD_SIZE;
		//*_tail -= mq->_block_size;
	}

	//	fourth, if tail space < 8
	else
	{
		//	copy tail byte
		memcpy(mq->_block+tail, sHead, tail_len);

		//	copy 8-tail byte
		unsigned second_len = C_HEAD_SIZE - tail_len;
		memcpy(mq->_block, sHead + tail_len, second_len);

		//	copy data
		memcpy(mq->_block + second_len, data, data_len);
		
		*mq->_tail = second_len + data_len;
	}
   (*mq->_enqueued_msg_cnt)++;
   if(free_len == mq->_block_size) 
      return 1;
   else
      return 0;
done:
   // release lock
	//_sem->post(mq->_sem_index);
   return ret;
}



int usnet_shmmq_enqueue(usn_shmmq_t *mq, 
      const time_t uiCurTime, const void* data, 
      unsigned data_len, unsigned flow)
{
   int ret = 0;
   unsigned int uiFactor;

   if ( mq == NULL )
      return -1;

   mq->_count++;

   ret = usnet_write_mq(mq, data, data_len, flow);
   if(ret < 0)
      return ret;

    usnet_adapctl_addload(mq->_adaptive_ctrl,uiCurTime, 1);
    uiFactor = mq->_adaptive_ctrl->m_uiFactor;
    if ( mq->_count == 1000 ) 
        printf("enqueue uiFactor=%d \n", uiFactor);
#ifdef USE_ADAPTIVE_CONTROL
    if (0 == _count%uiFactor)
#endif 
    {
       errno = 0;
       ret = write(mq->_fd, "\0", 1);
    }
#ifdef USE_ADAPTIVE_CONTROL
//    else
//    {
//	LOG(ERROR, "CFifoSyncMQ::enqueue: write to _fd not happen, _fd=%d, _count:%d, uiFactor:%d", _fd, _count, uiFactor);
//   }
#endif 

    if (1 == ret)
    {
        errno = 0;
        ret = write(mq->_fd, "\0", 1);
    }
    
    if (ret < 0 && errno != EAGAIN)
   	return -1;

    return 0;
}

int usnet_read_mq(usn_shmmq_t *mq, void* buf, unsigned buf_size, 
      unsigned *data_len, unsigned *flow)
{
   int ret = 0;
	char sHead[C_HEAD_SIZE];
	unsigned used_len;
	unsigned head = *mq->_head;
	unsigned tail = *mq->_tail;

   // get lock
	//_sem->wait(_sem_index);

	if (head == tail)
	{
		data_len = 0;
		ret = 0;
      goto done;
	}
   (*mq->_dequeued_msg_cnt)++;
	used_len = tail>head ? tail-head : tail+mq->_block_size-head;
	
	//	if head + 8 > block_size
	if (head+C_HEAD_SIZE > mq->_block_size)
	{
		unsigned first_size = mq->_block_size - head;
		unsigned second_size = C_HEAD_SIZE - first_size;
		memcpy(sHead, mq->_block + head, first_size);
		memcpy(sHead + first_size, mq->_block, second_size);
		head = second_size;
	}
	else
	{
		memcpy(sHead, mq->_block + head, C_HEAD_SIZE);
		head += C_HEAD_SIZE;
	}
	
	//	get meta data
	unsigned total_len  = *(unsigned*) (sHead);
	*flow = *(unsigned*) (sHead+sizeof(unsigned));

	assert(total_len <= used_len);
	
	*data_len = total_len-C_HEAD_SIZE;

	if (*data_len > buf_size)
   {
      printf("data_len is greater than buf_size, data_len=%d, buf_size=%d", *data_len, buf_size);
		ret = -1;
      goto done;
    }
	if (head+*data_len > mq->_block_size)	//	
	{
		unsigned first_size = mq->_block_size - head;
		unsigned second_size = *data_len - first_size;
		memcpy(buf, mq->_block + head, first_size);
		memcpy(((char*)buf) + first_size, mq->_block, second_size);
		*mq->_head = second_size;
	}
	else
	{
		memcpy(buf, mq->_block + head, *data_len);
		*mq->_head = head+*data_len;
	}
done:
   // release lock
	//_sem->post(_sem_index);
	return ret;
};

int usnet_shmmq_select_fifo(int _fd, unsigned _wait_sec, 
      unsigned _wait_usec)
{
   errno = 0;
   fd_set readfd;
   FD_ZERO(&readfd);
   FD_SET(_fd, &readfd);
   struct timeval tv;
   tv.tv_sec = _wait_sec;
   tv.tv_usec = _wait_usec;
    
   int ret = select(_fd+1, &readfd, NULL, NULL, &tv);
   if(ret > 0)
   {
      if(FD_ISSET(_fd, &readfd))
         return ret;
      else
         return -1;
   }
	else if (ret == 0)
	{
		return 0;
	}
	else
	{
		if (errno != EINTR)
		{
			close(_fd);
		}
		return -1;
	}
}

int usnet_shmmq_dequeue(usn_shmmq_t *mq, void* buf, 
      unsigned buf_size, unsigned *data_len, unsigned *flow)
{
   int ret;

   if ( mq == NULL )
      return -1;

	//	first, try to get data from queue.
   ret = usnet_read_mq(mq, buf, buf_size, data_len, flow); 
	if (ret || data_len)
		return ret;

	//	second, if no data, wait on fifo.
	ret = usnet_shmmq_select_fifo(mq->_fd, mq->_wait_sec, mq->_wait_usec);
	if (ret == 0) {
		data_len = 0;
		return ret;
	}
	else if (ret < 0) {
		return -1;
	}

	// third, if fifo activated, read the signals.
   {
	   static const unsigned buf_len = 1<<10;
   	char buffer[buf_len];
   	ret = read(mq->_fd, buffer, buf_len);
   	if (ret < 0 && errno != EAGAIN)
      {
         printf("read error, ret=%d, errno=%d", ret, errno);
         return -1;
      }
   }	
	//	fourth, get data
	return usnet_read_mq(mq, buf, buf_size, data_len, flow);
}


void usnet_shmmq_clear_flag(int _fd) 
{
    static char buffer[1];
    read(_fd, buffer, 1);
}




