#ifndef _USNET_IN_VAR_H_
#define _USNET_IN_VAR_H_

/*
 * Ip header, when holding a fragment.
 *
 * Note: ipf_next must be at same offset as ipq_next above
 */
struct   ipasfrag {
#if BYTE_ORDER == LITTLE_ENDIAN  
   u_char   ip_hl:4,
            ip_v:4; 
#endif 
#if BYTE_ORDER == BIG_ENDIAN 
   u_char   ip_v:4,
            ip_hl:4;
#endif
   u_char   ipf_mff;    /* XXX overlays ip_tos: use low bit
                         * to avoid destroying tos;
                         * copied from (ip_off&IP_MF) */
   short    ip_len;
   u_short  ip_id;
   short    ip_off;
   u_char   ip_ttl;
   u_char   ip_p;
   u_short  ip_sum;
   struct   ipasfrag *ipf_next;  /* next fragment */
   struct   ipasfrag *ipf_prev;  /* previous fragment */
};

/*
 * Ip reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
#define IPQ_TO_MBUF(fp)  (usn_mbuf_t*)(((u_char*)fp) - sizeof(struct ipq))
struct ipq {   
   struct ipq      *next;                 /* to other reass headers */
   u_char           ipq_ttl;              /* time for reass q to live */
   u_char           ipq_p;                /* protocol of this fragment */
   u_short          ipq_id;               /* sequence id for reassembly */
   usn_mbuf_t      *frags_list;           /* to ip headers of fragments */
   struct usn_in_addr   ipq_src,ipq_dst; 
};


extern struct ipq g_ipq;





#endif //!_USNET_IN_VAR_H_
