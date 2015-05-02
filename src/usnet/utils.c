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
 * @(#)utils.c
 */

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <ifaddrs.h>
#include <net/if_dl.h>


#include "utils.h"
#include "core.h"
#include "log.h"

void
dump_buffer(usn_context_t *ctx, char *p, int len, const char *prefix)
{
   char buf[128];
   int i, j, i0;

   if ( p == 0 ) {
      DEBUG(ctx->log,"null pointer");
      return;
   }
   /* get the length in ASCII of the length of the packet. */

   /* hexdump routine */
   for (i = 0; i < len; ) {
      memset(buf, sizeof(buf), ' ');
      sprintf(buf, "%5d: ", i);
      i0 = i; 
      for (j=0; j < 16 && i < len; i++, j++) 
         sprintf(buf+7+j*3, "%02x ", (uint8_t)(p[i]));
      i = i0;
      for (j=0; j < 16 && i < len; i++, j++) 
         sprintf(buf+7+j + 48, "%c",
            isprint(p[i]) ? p[i] : '.');
      DEBUG(ctx->log,"%s: %s", prefix, buf);
   }
}

int
convert_haddr_to_array(char* str, uint8_t *haddr)
{
   int values[6];
   int i;

   printf("mac addr: %s\n", str);
   if ( 6 == sscanf(str, "%x:%x:%x:%x:%x:%x",
            &values[0],&values[1],&values[2],
            &values[3],&values[4],&values[5]) )
   {
      for ( i=0; i<6; ++i) {
         haddr[i] = (uint8_t)values[i];
         printf("%d:%02x ", i, haddr[i]);
      }
      printf("\n");
      return 0;
   }

   return -1;
}

#undef  ADDCARRY
#define ADDCARRY(sum)  {\
         if (sum & 0xffff0000) {\
            sum &= 0xffff;\
            sum++;\
         }\
      }

unsigned short
cksum(usn_context_t *ctx, char* buf, int len) 
{
   uint32_t sum = 0;
   uint32_t cksum = 0;

   while ( len > 1 ) {
      sum = sum + *((uint16_t*)buf);
      buf += 2;
      len -= 2;
   }
   if ( len > 1 )
      sum = sum + *((uint8_t*)buf);

   while (sum >> 16)
      sum = (sum & 0xffff) + (sum >> 16);

   cksum = sum;
   DEBUG(ctx->log,"cksum: %hu", ~cksum);
   return ~cksum;
}

unsigned short 
in_cksum(usn_context_t *ctx, char* buf, int len) 
{
   union word {
      char  c[2];
      short  s;
   } u; 
   short *w;
   int sum = 0; 

   w = (short *) buf;
   if ((uintptr_t)w & 0x1) {
      DEBUG(ctx->log, "word is not align, %p", w);
   }
   while ((len -= 2) >= 0) {
      if ((uintptr_t)w & 0x1) {
         /* word is not aligned */
         u.c[0] = *(char *)w;
         u.c[1] = *((char *)w+1);
         sum += u.s;
         w++;
      } else
         sum += *w++;
      ADDCARRY(sum);
   }
   if (len == -1) {
      u.c[1] = 0;
      sum += u.s;
      ADDCARRY(sum);
   }
   DEBUG(ctx->log, "cksum: %u\n", ~sum & 0xffff);
   return (~sum & 0xffff);
}

uint16_t
tcp_cksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr)
{
   uint32_t sum;
   uint16_t *w; 
   int nleft;
   
   sum = 0;
   nleft = len;
   w = buf;
   
   while (nleft > 1)
   {   
      sum += *w++;
      nleft -= 2;
   }   
   
   // add padding for odd length
   if (nleft)
      sum += *w & ntohs(0xFF00);
   
   // add pseudo header
   sum += (saddr & 0x0000FFFF) + (saddr >> 16);
   sum += (daddr & 0x0000FFFF) + (daddr >> 16);
   sum += htons(len);
   sum += htons(IPPROTO_TCP);
   
   sum = (sum >> 16) + (sum & 0xFFFF);
   sum += (sum >> 16);
   
   sum = ~sum;
   
   return (uint16_t)sum;
}

uint16_t
udp_cksum(uint16_t *buf, uint16_t len, uint32_t saddr, uint32_t daddr)
{
   uint32_t sum;
   uint16_t *w; 
   int nleft;
   
   sum = 0;
   nleft = len;
   w = buf;
   
   while (nleft > 1)
   {   
      sum += *w++;
      nleft -= 2;
   }   
   
   // add padding for odd length
   if (nleft)
      sum += *w & ntohs(0xFF00);
   
   // add pseudo header
   sum += (saddr & 0x0000FFFF) + (saddr >> 16);
   sum += (daddr & 0x0000FFFF) + (daddr >> 16);
   sum += htons(len);
   sum += htons(IPPROTO_UDP);
   
   sum = (sum >> 16) + (sum & 0xFFFF);
   sum += (sum >> 16);
   
   sum = ~sum;
   
   return (uint16_t)sum;
}

char *state_str[] = {
   "TCPS_CLOSED",
   "TCPS_LISTEN",
   "TCPS_SYN_SENT",
   "TCPS_SYN_RCVD",
   "TCPS_ESTABILSHED",
   "TCPS_FIN_WAIT_1",
   "TCPS_FIN_WAIT_2",
   "TCPS_CLOSE_WAIT",
   "TCPS_CLOSING", 
   "TCPS_LAST_ACK", 
   "TCPS_TIME_WAIT"
}; 

inline char*
usnet_tcp_statestr(usn_tcb_t *tcb) 
{
   return state_str[tcb->state];
}

inline char*
usnet_tcp_statestr_new(int state) 
{
   return state_str[state];
}

int
usnet_net_hwaddr()
{
   struct ifaddrs *ifap, *ifaptr;
   unsigned char *ptr;

   if ( getifaddrs(&ifap) == 0 ) {
      for ( ifaptr=ifap; ifaptr!=0; ifaptr=(ifaptr)->ifa_next ) {
         if ( !strcmp((ifaptr)->ifa_name,"em1") && ((ifaptr->ifa_addr)->sa_family == AF_LINK) ) {
            ptr = (unsigned char*)LLADDR((struct sockaddr_dl*)(ifaptr)->ifa_addr);
            fprintf(stderr,"hwaddr: %02x:%02x:%02x:%02x:%02x:%02x\n", 
                  *ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5));
            break;
         }
      }
      freeifaddrs(ifap);
      return ifaptr != 0;
   } 

   return 0;
}


void
usnet_get_network_info(usn_context_t *ctx, const char *ifname, int len)
{
   int rsock;
   struct usn_ifnet *ifcfg;
   struct ifreq ifr;
   struct ifaddrs *ifap, *ifaptr;
   unsigned char *ptr;


   ifcfg = (struct usn_ifnet *)malloc(sizeof(*ifcfg));
   if ( ifcfg == 0 )
     return; 
   memset(ifcfg,0,sizeof(*ifcfg));
   ctx->ifnet = ifcfg;
   strncpy(ifcfg->iface,ifname,6); 

   rsock = socket(AF_INET,SOCK_STREAM,0); 

   if ( rsock < 0 )
      goto out;

#ifdef _USE_LINUX_
   memset(&ifr,0,sizeof(ifr));
   strncpy(ifr.ifr_name,ifcfg->iface,3);
   if ( ioctl(rsock,SIOCGIFHWADDR,&ifr) == -1 ) {
      goto out;
   }

   memcpy(&ifcfg->hwa,&ifr.ifr_hwaddr.sa_data,6);
#else
   if ( getifaddrs(&ifap) == 0 ) {
      for ( ifaptr=ifap; ifaptr!=0; ifaptr=(ifaptr)->ifa_next ) {
         if ( !strcmp((ifaptr)->ifa_name,ifname) && ((ifaptr->ifa_addr)->sa_family == AF_LINK) ) {
            ptr = (unsigned char*)LLADDR((struct sockaddr_dl*)(ifaptr)->ifa_addr);
            //fprintf(stderr,"hwaddr: %02x:%02x:%02x:%02x:%02x:%02x\n", 
            //      *ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5));
            memcpy(ifcfg->hwa,ptr,ETH_LEN);
            break;
         }
      }
      freeifaddrs(ifap);
   } 

#endif
   memset(&ifr,0,sizeof(ifr));
   strncpy(ifr.ifr_name,ifcfg->iface,3);
   if ( ioctl(rsock,SIOCGIFADDR,&ifr) == -1 ) {
      goto out;
   }

   memcpy(&ifcfg->addr,&(*(struct sockaddr_in*)&ifr.ifr_addr).sin_addr,4);

   memset(&ifr,0,sizeof(ifr));
   strncpy(ifr.ifr_name,ifcfg->iface,3);
   if ( ioctl(rsock,SIOCGIFBRDADDR,&ifr) == -1 ) {
      goto out;
   }

   memcpy(&ifcfg->bcast,&(*(struct sockaddr_in*)&ifr.ifr_broadaddr).sin_addr,4);

   memset(&ifr,0,sizeof(ifr));
   strncpy(ifr.ifr_name,ifcfg->iface,3);
   if ( ioctl(rsock,SIOCGIFNETMASK,&ifr) == -1 ) {
      goto out;
   }

   memcpy(&ifcfg->nmask,&(*(struct sockaddr_in*)&ifr.ifr_addr).sin_addr,4);

   memset(&ifr,0,sizeof(ifr));
   strncpy(ifr.ifr_name,ifcfg->iface,3);
   if ( ioctl(rsock,SIOCGIFMTU,&ifr) == -1 ) {
      goto out;
   }
   ifcfg->mtu = ifr.ifr_mtu;

out:   
   close(rsock);
}


