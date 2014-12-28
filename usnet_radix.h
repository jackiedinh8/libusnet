#ifndef _USNET_RADIX_H_
#define _USNET_RADIX_H_

#include <strings.h>
#include "usnet_types.h"

extern int	g_max_keylen;
extern struct usn_radix_mask *rn_mkfreelist;
extern struct usn_radix_node_head *mask_rnhead;
extern char *addmask_key;
extern char normal_chars[];
extern char *rn_zeros, *rn_ones;


/*
 * Radix search tree node layout.
 */

struct usn_radix_node {
	struct	usn_radix_mask *rn_mklist;	// list of masks contained in subtree //
	struct	usn_radix_node *rn_p;	// parent //
	short	rn_b;			// bit offset; -1-index(netmask) //
	char	rn_bmask;		// node: mask for bit test//
	u_char	rn_flags;		// enumerated next //
#define RNF_NORMAL	1		// leaf contains normal route //
#define RNF_ROOT	2		// leaf is root leaf for tree //
#define RNF_ACTIVE	4		// This node is alive (for rtfree) //
	union {
		struct {			// leaf only data: //
			caddr_t	rn_Key;	// object of search //
			caddr_t	rn_Mask;	// netmask, if present //
			struct	usn_radix_node *rn_Dupedkey;
		} rn_leaf;
		struct {			// node only data: //
			int	rn_Off;		// where to start compare //
			struct	usn_radix_node *rn_L;// progeny //
			struct	usn_radix_node *rn_R;// progeny //
		}rn_node;
	}		rn_u;
#ifdef RN_DEBUG
	int rn_info;
	struct usn_radix_node *rn_twin;
	struct usn_radix_node *rn_ybro;
#endif
};


#define rn_dupedkey rn_u.rn_leaf.rn_Dupedkey
#define rn_key rn_u.rn_leaf.rn_Key
#define rn_mask rn_u.rn_leaf.rn_Mask
#define rn_off rn_u.rn_node.rn_Off
#define rn_l rn_u.rn_node.rn_L
#define rn_r rn_u.rn_node.rn_R

/*
 * Annotations to tree concerning potential routes applying to subtrees.
 */

extern struct usn_radix_mask {
	short	rm_b;			// bit offset; -1-index(netmask) //
	char	rm_unused;		// cf. rn_bmask //
	u_char	rm_flags;		// cf. rn_flags //
	struct	usn_radix_mask *rm_mklist;	// more masks to try //
	union	{
		caddr_t	rmu_mask;		// the mask //
		struct	usn_radix_node *rmu_leaf;	// for normal routes //
	}	rm_rmu;
	int	rm_refs;		// # of references to this struct //
} *rn_mkfreelist;

#define rm_mask rm_rmu.rmu_mask
#define rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */

#define MKGet(m) {\
	if (rn_mkfreelist) {\
		m = rn_mkfreelist; \
		rn_mkfreelist = (m)->rm_mklist; \
	} else \
		R_Malloc(m, struct usn_radix_mask *, sizeof (*(m))); }\

#define MKFree(m) { (m)->rm_mklist = rn_mkfreelist; rn_mkfreelist = (m);}

struct usn_radix_node_head {
	struct	usn_radix_node *rnh_treetop;
	int	rnh_addrsize;		/* permit, but not require fixed keys */
	int	rnh_pktsize;		/* permit, but not require fixed keys */
	struct	usn_radix_node *(*rnh_addaddr)	/* add based on sockaddr */
		(void *v, void *mask,
		     struct usn_radix_node_head *head, struct usn_radix_node nodes[]);
	struct	usn_radix_node *(*rnh_addpkt)	/* add based on packet hdr */
		(void *v, void *mask,
		     struct usn_radix_node_head *head, struct usn_radix_node nodes[]);
	struct	usn_radix_node *(*rnh_deladdr)	/* remove based on sockaddr */
		(void *v, void *mask, struct usn_radix_node_head *head);
	struct	usn_radix_node *(*rnh_delpkt)	/* remove based on packet hdr */
		(void *v, void *mask, struct usn_radix_node_head *head);
	struct	usn_radix_node *(*rnh_matchaddr)	/* locate based on sockaddr */
		(void *v, struct usn_radix_node_head *head);
	struct	usn_radix_node *(*rnh_lookup)	/* locate based on sockaddr */
		(void *v, void *mask, struct usn_radix_node_head *head);
	struct	usn_radix_node *(*rnh_matchpkt)	/* locate based on packet hdr */
		(void *v, struct usn_radix_node_head *head);
	int	(*rnh_walktree)			/* traverse tree */
		(struct usn_radix_node_head *head, int (*f)(struct usn_radix_node*, void*), void *);
	struct	usn_radix_node rnh_nodes[3];	/* empty tree for common case */
};

#define Bcmp(a, b, n) bcmp(((char *)(a)), ((char *)(b)), (n))
#define Bcopy(a, b, n) bcopy(((char *)(a)), ((char *)(b)), (unsigned)(n))
#define Bzero(p, n) bzero((char *)(p), (int)(n));
#define R_Malloc(p, t, n) (p = (t) usn_get_buf(0,(unsigned int)(n)))
#define Free(p) usn_free_buf((u_char *)p);

void	 rn1_init (void);
int	 rn1_inithead (void **, int);
int	 rn1_refines (void *, void *);
int	 rn1_walktree (struct usn_radix_node_head *, int (*f)(struct usn_radix_node*, void*), void *);
struct usn_radix_node
	 *rn1_addmask (void *, int, int),
	 *rn1_addroute (void *, void *, struct usn_radix_node_head *,
			struct usn_radix_node [2]),
	 *rn1_delete (void *, void *, struct usn_radix_node_head *),
	 *rn1_insert (void *, struct usn_radix_node_head *, int *,
			struct radix_node [2]),
	 *rn1_match (void *, struct usn_radix_node_head *),
	 *rn1_newpair (void *, int, struct usn_radix_node[2]),
	 *rn1_search (void *, struct usn_radix_node *),
	 *rn1_search_m (void *, struct usn_radix_node *, void *),
    *rn1_lookup(void *, void*, struct usn_radix_node_head*);


int print_radix_head(struct usn_radix_node_head* h);

#endif //!_USNET_RADIX_H_ 
