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
 * @(#)mq.c
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#ifdef linux
#include <time.h>
#endif

#include "mq.h"
#include "log.h"

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


int 
usnet_init_shmmq(usn_shmmq_t *mq, char* fifo_path, 
      int32_t wait_sec, int32_t wait_usec, 
      int32_t shm_key, int32_t shm_size, int32_t sync)
{
   int ret = 0;
   int val;
   char *mem_addr = NULL;
   int mode = 0666 | O_NONBLOCK | O_NDELAY;

   if ( mq == NULL )  {
      //ERROR("null pointer, addr=%p",  mq); 
      return -1;
   }

	errno = 0;
   if ((mkfifo(fifo_path, mode)) < 0)
		if (errno != EEXIST) {
         //ERROR("failed to make fifo, file=%s",  fifo_path); 
			ret = -1;
         goto done;
      }

   if ((mq->_fd = open(fifo_path, O_RDWR)) < 0) {
		ret = -2;
      //ERROR("failed to open fifo, file=%s",  fifo_path); 
      goto done;
   }

	if (mq->_fd > 1024)
	{
		close(mq->_fd);
		ret = -3;
      //ERROR("fd is too small, fd=%d", mq->_fd); 
      goto done;
	}
    
	val = fcntl(mq->_fd, F_GETFL, 0);
	
	if (val == -1) {
		ret = errno ? -errno : val;
      //ERROR("failed to get fl, fd=%d", mq->_fd); 
      goto done;
   }

	if (val & O_NONBLOCK) {
		ret = 0;
      goto done;
   }
	
	ret = fcntl(mq->_fd, F_SETFL, val | O_NONBLOCK | O_NDELAY);

	if (ret < 0) {
      ret = errno ? -errno : ret;
      //ERROR("failed to set fl, fd=%d, ret", mq->_fd, ret); 
      goto done;
   } else
      ret = 0;

	assert(shm_size > C_HEAD_SIZE);

	mq->_shm = usnet_shm_create(shm_key, shm_size);

   if ( mq->_shm == NULL ) {
      mq->_shm = usnet_shm_open(shm_key, shm_size);
      if ( mq->_shm == NULL ) {
		   ret = -1;
         //ERROR("failed to open shm, key=%d, size=%d", shm_key, shm_size);
         goto done;
   	}
      mem_addr = mq->_shm->addr;
      goto setup;
	} else
      mem_addr = mq->_shm->addr;

   // init head portion of shared meme.
   memset(mem_addr, 0, C_HEAD_SIZE * 2 + sizeof(*mq->_adaptive_ctrl));

   // init adaptive control.
   mq->_adaptive_ctrl = (usn_adapctl_t *)mem_addr;
   mq->_adaptive_ctrl->m_uiCheckTimeSpan = 1;
   mq->_adaptive_ctrl->m_uiMsgCount = 0;
   mq->_adaptive_ctrl->m_uiLastCheckTime = time(NULL);
   mq->_adaptive_ctrl->m_uiLastFactor = 1;
   mq->_adaptive_ctrl->m_uiFactor = 1;
   mq->_adaptive_ctrl->m_uiSync = sync;

	mq->_wait_sec = wait_sec;
	mq->_wait_usec = wait_usec;
 
setup:
   mq->_adaptive_ctrl = (usn_adapctl_t *)mem_addr;
   mem_addr += sizeof(*mq->_adaptive_ctrl);
   mq->_enqueued_msg_cnt = (uint32_t*)mem_addr;
   mq->_dequeued_msg_cnt = (uint32_t*)mem_addr + 1;

   //	set head and tail
	mq->_head = (uint32_t*)mem_addr + 2;
	mq->_tail = mq->_head+1;
	mq->_block = (char*) (mq->_tail+1);
	mq->_block_size = shm_size - ( C_HEAD_SIZE * 2 + sizeof(*mq->_adaptive_ctrl) );

	ret = 0;
done:
   return ret;
}

void 
usnet_release_shmmq(usn_shmmq_t *mq)
{
   // TODO
}

int
usnet_write_mq(usn_shmmq_t *mq, const void* data, int32_t data_len, int32_t flow)
{
	uint32_t head;// = *_head;
	uint32_t tail;// = *_tail;	
	uint32_t free_len;// = head>tail? head-tail: head+_block_size-tail;
	uint32_t tail_len;// = _block_size - tail;
	char sHead[C_HEAD_SIZE] = {0};
	uint32_t total_len;// = data_len+C_HEAD_SIZE;
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

	memcpy(sHead, &total_len, sizeof(uint32_t));
	memcpy(sHead+sizeof(uint32_t), &flow, sizeof(uint32_t));

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
		uint32_t first_len = tail_len - C_HEAD_SIZE;
		memcpy(mq->_block+tail+ C_HEAD_SIZE, data, first_len);

		//	copy left
		uint32_t second_len = data_len - first_len;
		memcpy(mq->_block, ((char*)data) + first_len, second_len);

        // XXX
		int32_t itmp = *mq->_tail + data_len + C_HEAD_SIZE - mq->_block_size;
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
		uint32_t second_len = C_HEAD_SIZE - tail_len;
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



int 
usnet_shmmq_enqueue(usn_shmmq_t *mq, 
      const time_t uiCurTime, const void* data, 
      uint32_t data_len, uint32_t flow)
{
   int ret = 0;

   if ( mq == NULL )
      return -1;

   mq->_count++;

   ret = usnet_write_mq(mq, data, data_len, flow);
   if(ret < 0)
      return ret;

   usnet_adapctl_addload(mq->_adaptive_ctrl,uiCurTime, 1);

#ifdef USE_ADAPTIVE_CONTROL
   if (0 == mq->_count% mq->_adaptive_ctrl->m_uiFactor)
#endif 
   {
      errno = 0;
      ret = write(mq->_fd, "\0", 1);
   }
   return 0;
}

int
usnet_read_mq(usn_shmmq_t *mq, void* buf, uint32_t buf_size, 
     int32_t *data_len, uint32_t *flow)
{
   int ret = 0;
	char sHead[C_HEAD_SIZE];
	uint32_t used_len;
   uint32_t total_len;
	uint32_t head = *mq->_head;
	uint32_t tail = *mq->_tail;

   // get lock
	//_sem->wait(_sem_index);

	if (head == tail)
	{
		*data_len = 0;
		ret = 0;
      goto done;
	}
   (*mq->_dequeued_msg_cnt)++;
	used_len = tail>head ? tail-head : tail+mq->_block_size-head;
	
	//	if head + 8 > block_size
	if (head+C_HEAD_SIZE > mq->_block_size)
	{
		uint32_t first_size = mq->_block_size - head;
		uint32_t second_size = C_HEAD_SIZE - first_size;
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
	total_len  = *(uint32_t*) (sHead);
	*flow = *(uint32_t*) (sHead+sizeof(uint32_t));

	assert(total_len <= used_len);
	
	*data_len = total_len-C_HEAD_SIZE;

	if (*data_len > buf_size)
   {
      //ERROR("data_len is greater than buf_size, data_len=%d, buf_size=%d", *data_len, buf_size);
		ret = -1;
      goto done;
    }
	if (head+*data_len > mq->_block_size)	//	
	{
		uint32_t first_size = mq->_block_size - head;
		uint32_t second_size = *data_len - first_size;
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

int 
usnet_shmmq_dequeue(usn_shmmq_t *mq, void* buf, 
      uint32_t buf_size, int32_t *data_len, uint32_t *flow)
{
   int ret;

   if ( mq == NULL )
      return -1;

	//	first, try to get data from queue.
   ret = usnet_read_mq(mq, buf, buf_size, data_len, flow); 
	if (ret || *data_len)
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
	   static const int32_t buf_len = 1<<10;
   	u_char buffer[buf_len];
   	ret = read(mq->_fd, buffer, buf_len);
   	if (ret < 0 && errno != EAGAIN)
      {
         //ERROR("read error, ret=%d, errno=%d", ret, errno);
         return -1;
      }
   }	
	//	fourth, get data
	ret = usnet_read_mq(mq, buf, buf_size, data_len, flow);

   return ret;
}

int usnet_shmmq_select_fifo_wait(int _fd, unsigned _wait_sec, 
      unsigned _wait_usec)
{
   errno = 0;
   fd_set readfd;
   FD_ZERO(&readfd);
   FD_SET(_fd, &readfd);
   struct timeval tv;
   tv.tv_sec = _wait_sec;
   tv.tv_usec = _wait_usec;
    
   //int ret = select(_fd+1, &readfd, NULL, NULL, &tv);
   int ret = select(_fd+1, &readfd, NULL, NULL, 0); (void)tv;
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

int 
usnet_shmmq_dequeue_wait(usn_shmmq_t *mq, void* buf, 
      uint32_t buf_size, int32_t *data_len, uint32_t *flow)
{
   int ret;

   if ( mq == NULL )
      return -1;

	//	first, try to get data from queue.
   //ret = usnet_read_mq(mq, buf, buf_size, data_len, flow); 
	//if (ret || *data_len)
	//	return ret;

	//	second, if no data, wait on fifo.
	ret = usnet_shmmq_select_fifo_wait(mq->_fd, 1, 0);
	if (ret == 0) {
		data_len = 0;
		return ret;
	}
	else if (ret < 0) {
		return -1;
	}

	// third, if fifo activated, read the signals.
   {
	   static const int32_t buf_len = 1;//<<10;
   	u_char buffer[buf_len];
   	ret = read(mq->_fd, buffer, buf_len);
   	if (ret < 0 && errno != EAGAIN)
      {
         //ERROR("read error, ret=%d, errno=%d", ret, errno);
         return -1;
      }
   }	

	//	fourth, get data
reread:
	ret = usnet_read_mq(mq, buf, buf_size, data_len, flow);
   if ( *data_len == 0 ) {
      usleep(500); // FIXME: config time_wait here.
      goto reread;
   }

   return ret;
}

void 
usnet_shmmq_clear_flag(uint32_t _fd) 
{
    static u_char buffer[1];
    read(_fd, buffer, 1);
}




