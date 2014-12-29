/*
 * Routines to build and maintain radix trees for routing lookups.
 */

#include "usnet_radix.h"
#include "usnet_core.h"
#include "usnet_log.h"
#include "usnet_in.h"
#include "usnet_buf.h"
#include "usnet_route.h"

int	g_max_keylen;
struct usn_radix_mask *rn_mkfreelist;
struct usn_radix_node_head *mask_rnhead;
char normal_chars[] = {(char)0, (char)0x80, (char)0xc0, (char)0xe0, (char)0xf0, (char)0xf8, (char)0xfc, (char)0xfe, (char)-1};
char *rn_zeros, *rn_ones;
char *addmask_key;

#define rn_masktop (mask_rnhead->rnh_treetop)
#undef Bcmp
#define Bcmp(a, b, l) (l == 0 ? 0 : bcmp((caddr_t)(a), (caddr_t)(b), (u_long)l))
/*
 * The data structure for the keys is a radix tree with one way
 * branching removed.  The index rn_b at an internal node n represents a bit
 * position to be tested.  The tree is arranged so that all descendants
 * of a node n have keys whose bits all agree up to position rn_b - 1.
 * (We say the index of n is rn_b.)
 *
 * There is at least one descendant which has a one bit at position rn_b,
 * and at least one with a zero there.
 *
 * A route is determined by a pair of key and mask.  We require that the
 * bit-wise logical and of the key and mask to be the key.
 * We define the index of a route to associated with the mask to be
 * the first bit number in the mask where 0 occurs (with bit number 0
 * representing the highest order bit).
 * 
 * We say a mask is normal if every bit is 0, past the index of the mask.
 * If a node n has a descendant (k, m) with index(m) == index(n) == rn_b,
 * and m is a normal mask, then the route applies to every descendant of n.
 * If the index(m) < rn_b, this implies the trailing last few bits of k
 * before bit b are all 0, (and hence consequently true of every descendant
 * of n), so the route applies to all descendants of the node as well.
 * 
 * Similar logic shows that a non-normal mask m such that
 * index(m) <= index(n) could potentially apply to many children of n.
 * Thus, for each non-host route, we attach its mask to a list at an internal
 * node as high in the tree as we can go. 
 *
 * The present version of the code makes use of normal routes in short-
 * circuiting an explict mask and compare operation when testing whether
 * a key satisfies a normal route, and also in remembering the unique leaf
 * that governs a subtree.
 */

struct usn_radix_node *
rn1_search(void *v_arg, struct usn_radix_node *head)
{
	struct usn_radix_node *x;
	caddr_t v;

	for (x = head, v = (caddr_t)v_arg; x->rn_b >= 0;) {
		if (x->rn_bmask & v[x->rn_off])
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return (x);
};

struct usn_radix_node *
rn1_search_m( void *v_arg, struct usn_radix_node *head, void *m_arg)
{
	struct usn_radix_node *x;
	caddr_t v = (caddr_t)v_arg, m = (caddr_t)m_arg;

	for (x = head; x->rn_b >= 0;) {
		if ((x->rn_bmask & m[x->rn_off]) &&
		    (x->rn_bmask & v[x->rn_off]))
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return x;
};

int
rn1_refines( void *m_arg, void *n_arg)
{
	caddr_t m = (caddr_t)m_arg, n = (caddr_t)n_arg;
	caddr_t lim, lim2 = lim = n + *(u_char *)n;
	int longer = (*(u_char *)n++) - (int)(*(u_char *)m++);
	int masks_are_equal = 1;

	if (longer > 0)
		lim -= longer;
	while (n < lim) {
		if (*n & ~(*m))
			return 0;
		if (*n++ != *m++)
			masks_are_equal = 0;
	}
	while (n < lim2)
		if (*n++)
			return 0;
	if (masks_are_equal && (longer < 0))
		for (lim2 = m - longer; m < lim2; )
			if (*m++)
				return 1;
	return (!masks_are_equal);
}

struct usn_radix_node *
rn1_lookup( void *v_arg, void *m_arg, struct usn_radix_node_head *head)
{
	struct usn_radix_node *x;
	caddr_t netmask = 0;

	if (m_arg) {
		if ((x = rn1_addmask(m_arg, 1, head->rnh_treetop->rn_off)) == 0)
			return (0);
		netmask = x->rn_key;
	}
	x = rn1_match(v_arg, head);
	if (x && netmask) {
		while (x && x->rn_mask != netmask)
			x = x->rn_dupedkey;
	}
	return x;
}

#define min(x,y) ((x) < (y) ? (x) : (y))
static int
rn1_satsifies_leaf( char *trial, struct usn_radix_node *leaf, int skip)
{
	char *cp = trial, *cp2 = leaf->rn_key, *cp3 = leaf->rn_mask;
	char *cplim;
	int length = min(*(u_char *)cp, *(u_char *)cp2);

	if (cp3 == 0)
		cp3 = rn_ones;
	else
		length = min(length, *(u_char *)cp3);
	cplim = cp + length; cp3 += skip; cp2 += skip;
	for (cp += skip; cp < cplim; cp++, cp2++, cp3++)
		if ((*cp ^ *cp2) & *cp3)
			return 0;
	return 1;
}

void print_duped_key(struct usn_radix_node *t)
{
   const char   *key = "rn_key ";
   const char   *mask = "rn_mask";
   if ( t == NULL)
     return;

   DEBUG("key: rn_b=%d, rn_bmask=%u, rn_flags=%d, rn_key=%x",
        t->rn_b, (u_char)t->rn_bmask, t->rn_flags, *((u_int*)t->rn_key));
   dump_buffer((char*)t->rn_key,*(char*)t->rn_key, key);
   if ( t->rn_mask ) 
      dump_buffer((char*)t->rn_mask,*((char*)t->rn_mask), mask);

   {
         struct rtentry *rt;
         const char   *gate = "gateway";
         rt = (struct rtentry *)t;
         if ( rt->rt_gateway )
            dump_buffer((char*)rt->rt_gateway,*((char*)rt->rt_gateway), gate);
   }

   print_duped_key(t->rn_dupedkey);
   return;
}
void print_radix_mask(struct usn_radix_mask *m)
{
   if ( m == NULL)
     return;
   DEBUG("mask: rm_b=%d, rm_unused=%d, rm_flags=%d", m->rm_b, m->rm_unused, m->rm_flags);
   print_radix_mask(m->rm_mklist);
   return;
}

int print_radix_tree(struct usn_radix_node* t)
{  
   const char   *key = "rn_key ";
   const char   *mask = "rn_mask";
	if ( t->rn_b < 0 ) {
      DEBUG("leaf: ptr=%p, rn_b=%d, rn_bmask=%u, rn_flags=%d, rn_key=%x, rn_mask=%x",
           t, t->rn_b, (u_char)t->rn_bmask, t->rn_flags, *((u_int*)t->rn_key), 
           (t->rn_mask == 0 ) ? 0 : *((u_int*)t->rn_mask));
      dump_buffer((char*)t->rn_key,*((char*)t->rn_key), key);
      if ( t->rn_mask ) 
         dump_buffer((char*)t->rn_mask,*((char*)t->rn_mask), mask);

      if ( (t->rn_flags & RNF_ROOT) == 0 ) {
         struct rtentry *rt;
         const char   *gate = "gateway";
         rt = (struct rtentry *)t;
         if ( rt->rt_gateway )
            dump_buffer((char*)rt->rt_gateway,*((char*)rt->rt_gateway), gate);
      }

      print_duped_key(t->rn_dupedkey);
   } else {
#ifdef DUMP_PAYLOAD
      DEBUG("node: ptr=%p, rn_b=%d, rn_bmask=%u, rn_flags=%d, rn_off=%x",
           t, t->rn_b, (u_char)t->rn_bmask, t->rn_flags, t->rn_off);
      dump_buffer((char*)t, sizeof(*t), "rdx");
#endif
      print_radix_mask(t->rn_mklist);
      print_radix_tree(t->rn_l);
      print_radix_tree(t->rn_r);
   }
   return 0;
}

int print_radix_head(struct usn_radix_node_head* h)
{
   print_radix_tree(h->rnh_treetop);
   return 0;
}

int print_radix_node(struct usn_radix_node* t, void* arg)
{
   DEBUG("node: rn_b=%d, rn_bmask=%d, rn_flags=%d, rn_off=%x",
        t->rn_b, t->rn_bmask, t->rn_flags, t->rn_off);
   return 0;
}

int print_radix_leaf(struct usn_radix_node* n, void* arg)
{
   DEBUG("leaf: rn_b=%d, rn_bmask=%x, rn_flags=%d, rn_key=%x",
           n->rn_b, n->rn_bmask, n->rn_flags, *((u_int*)n->rn_key));
   return 0;
}
struct usn_radix_node *
rn1_match( void *v_arg, struct usn_radix_node_head *head)
{
	caddr_t v = (caddr_t)v_arg;
	struct usn_radix_node *t = head->rnh_treetop, *x;
	caddr_t cp = v, cp2;
	caddr_t cplim;
	struct usn_radix_node *saved_t, *top = t;
	int off = t->rn_off, vlen = *(u_char *)cp, matched_off;
	int test, b, rn_b;

   //print_radix_tree(t);

	/*
	 * Open code rn_search(v, top) to avoid overhead of extra
	 * subroutine call.
	 */
	for (; t->rn_b >= 0; ) {
		if (t->rn_bmask & cp[t->rn_off])
			t = t->rn_r;
		else
			t = t->rn_l;
	}

	/*
	 * See if we match exactly as a host destination
	 * or at least learn how many bits match, for normal mask finesse.
	 *
	 * It doesn't hurt us to limit how many bytes to check
	 * to the length of the mask, since if it matches we had a genuine
	 * match and the leaf we have is the most specific one anyway;
	 * if it didn't match with a shorter length it would fail
	 * with a long one.  This wins big for class B&C netmasks which
	 * are probably the most common case...
	 */
	if (t->rn_mask) {
#ifdef DUMP_PAYLOAD
      DEBUG("dump rn_mask");
      dump_buffer(t->rn_mask, *((u_char*)t->rn_mask), "rdx");
#endif
		vlen = *(u_char *)t->rn_mask;
   }

#ifdef DUMP_PAYLOAD
   DEBUG("dump rn_key");
   dump_buffer(t->rn_key, g_max_keylen, "rdx");
#endif

	cp += off; cp2 = t->rn_key + off; cplim = v + vlen;
	for (; cp < cplim; cp++, cp2++)
		if (*cp != *cp2)
			goto on1;
	/*
	 * This extra grot is in case we are explicitly asked
	 * to look up the default.  Ugh!
	 */
	if ((t->rn_flags & RNF_ROOT) && t->rn_dupedkey)
		t = t->rn_dupedkey;
	return t;
on1:
	test = (*cp ^ *cp2) & 0xff; /* find first bit that differs */
	for (b = 7; (test >>= 1) > 0;)
		b--;
	matched_off = cp - v;
	b += matched_off << 3;
	rn_b = -1 - b;
	/*
	 * If there is a host route in a duped-key chain, it will be first.
	 */
	if ((saved_t = t)->rn_mask == 0)
		t = t->rn_dupedkey;
	for (; t; t = t->rn_dupedkey)
		/*
		 * Even if we don't match exactly as a host,
		 * we may match if the leaf we wound up at is
		 * a route to a net.
		 */
		if (t->rn_flags & RNF_NORMAL) {
			if (rn_b <= t->rn_b)
				return t;
		} else if (rn1_satsifies_leaf(v, t, matched_off))
				return t;
	t = saved_t;
	/* start searching up the tree */
	do {
		struct usn_radix_mask *m;
		t = t->rn_p;
		m = t->rn_mklist;
		if (m) {
			/*
			 * If non-contiguous masks ever become important
			 * we can restore the masking and open coding of
			 * the search and satisfaction test and put the
			 * calculation of "off" back before the "do".
			 */
			do {
				if (m->rm_flags & RNF_NORMAL) {
					if (rn_b <= m->rm_b)
						return (m->rm_leaf);
				} else {
					off = min(t->rn_off, matched_off);
					x = rn1_search_m(v, t, m->rm_mask);
					while (x && x->rn_mask != m->rm_mask)
						x = x->rn_dupedkey;
					if (x && rn1_satsifies_leaf(v, x, off))
						    return x;
				}
			   m = m->rm_mklist;
			} while (m);
		}
	} while (t != top);
	return 0;
};
		
//#define RN_DEBUG
#ifdef RN_DEBUG
int	rn_nodenum;
struct	usn_radix_node *rn_clist;
int	rn_saveinfo;
int	rn_debug =  1;
#endif

struct usn_radix_node *
rn1_newpair( void *v, int b, struct usn_radix_node nodes[2])
{
	struct usn_radix_node *tt = nodes, *t = tt + 1;

	t->rn_b = b; t->rn_bmask = 0x80 >> (b & 7);
	t->rn_l = tt; t->rn_off = b >> 3;

	tt->rn_b = -1; tt->rn_key = (caddr_t)v; tt->rn_p = t;
	tt->rn_flags = t->rn_flags = RNF_ACTIVE;

#ifdef RN_DEBUG
	tt->rn_info = rn_nodenum++; t->rn_info = rn_nodenum++;
	tt->rn_twin = t; tt->rn_ybro = rn_clist; rn_clist = tt;
#endif
	return t;
}

struct usn_radix_node *
rn1_insert( void *v_arg, struct usn_radix_node_head *head, int *dupentry, struct usn_radix_node nodes[2])
{
	caddr_t v = (caddr_t)v_arg;
	struct usn_radix_node *top = head->rnh_treetop;
	int head_off = top->rn_off, vlen = (int)*((u_char *)v);
	struct usn_radix_node *t = rn1_search(v_arg, top);
	caddr_t cp = v + head_off;
	int b;
	struct usn_radix_node *tt;
  	/*
	 * Find first bit at which v and t->rn_key differ
	 */
   {
	   caddr_t cp2 = t->rn_key + head_off;
   	int cmp_res;
   	caddr_t cplim = v + vlen;

   	while (cp < cplim)
   		if (*cp2++ != *cp++)
   			goto on1;
   	*dupentry = 1;
   	return t;
on1:
   	*dupentry = 0;
   	cmp_res = (cp[-1] ^ cp2[-1]) & 0xff;
   	for (b = (cp - v) << 3; cmp_res; b--)
   		cmp_res >>= 1;
      // b is number of bits from the start of v to the different bit.
   }

   // block code
   {
	   struct usn_radix_node *p, *x = top;
   	cp = v;
   	do {
   		p = x;
   		if (cp[x->rn_off] & x->rn_bmask) 
   			x = x->rn_r;
   		else 
            x = x->rn_l;
         DEBUG("b=%d, rn_b=%d", b, x->rn_b);
   	} while ((u_int)b > (u_int) x->rn_b); /* x->rn_b < b && x->rn_b >= 0 */
#ifdef RN_DEBUG
   	if (rn_debug)
   		DEBUG("rn_insert: Going In:\n"), traverse(p);
#endif
   	t = rn1_newpair(v_arg, b, nodes); tt = t->rn_l;

   	if ((cp[p->rn_off] & p->rn_bmask) == 0)
   		p->rn_l = t;
   	else
   		p->rn_r = t;

   	x->rn_p = t; t->rn_p = p; /* frees x, p as temp vars below */
   	if ((cp[t->rn_off] & t->rn_bmask) == 0) {
   		t->rn_r = x;
   	} else {
   		t->rn_r = tt; t->rn_l = x;
   	}
#ifdef RN_DEBUG
   	if (rn_debug)
   		DEBUG("rn_insert: Coming Out:\n"), traverse(p);
#endif
   }
	return (tt);
}

struct usn_radix_node *
rn1_addmask(void *n_arg, int search, int skip)
{
	caddr_t netmask = (caddr_t)n_arg;
	struct usn_radix_node *x;
	caddr_t cp, cplim;
	int b = 0, mlen, j;
	int maskduplicated, m0, isnormal;
	struct usn_radix_node *saved_x;
	static int last_zeroed = 0;

	if ((mlen = *(u_char *)netmask) > g_max_keylen)
		mlen = g_max_keylen;
	if (skip == 0)
		skip = 1;
	if (mlen <= skip)
		return (mask_rnhead->rnh_nodes);
	if (skip > 1)
		Bcopy(rn_ones + 1, addmask_key + 1, skip - 1);
	if ((m0 = mlen) > skip)
		Bcopy(netmask + skip, addmask_key + skip, mlen - skip);
	/*
	 * Trim trailing zeroes.
	 */
	for (cp = addmask_key + mlen; (cp > addmask_key) && cp[-1] == 0;)
		cp--;
	mlen = cp - addmask_key;
	if (mlen <= skip) {
		if (m0 >= last_zeroed)
			last_zeroed = mlen;
		return (mask_rnhead->rnh_nodes);
	}
	if (m0 < last_zeroed)
		Bzero(addmask_key + m0, last_zeroed - m0);
	*addmask_key = last_zeroed = mlen;
	x = rn1_search(addmask_key, rn_masktop);
	if (Bcmp(addmask_key, x->rn_key, mlen) != 0)
		x = 0;
	if (x || search)
		return (x);
	R_Malloc(x, struct usn_radix_node *, g_max_keylen + 2 * sizeof (*x));
	if ((saved_x = x) == 0)
		return (0);
	Bzero(x, g_max_keylen + 2 * sizeof (*x));
	netmask = cp = (caddr_t)(x + 2);
	Bcopy(addmask_key, cp, mlen);
	x = rn1_insert(cp, mask_rnhead, &maskduplicated, x);
	if (maskduplicated) {
		DEBUG("rn_addmask: mask impossibly already in tree");
		Free(saved_x);
		return (x);
	}
	/*
	 * Calculate index of mask, and check for normalcy.
	 */
	cplim = netmask + mlen; isnormal = 1;
	for (cp = netmask + skip; (cp < cplim) && *(u_char *)cp == 0xff;)
		cp++;
	if (cp != cplim) {
		for (j = 0x80; (j & *cp) != 0; j >>= 1)  
			b++;
		if (*cp != normal_chars[b] || cp != (cplim - 1))
			isnormal = 0;
	}
	b += (cp - netmask) << 3;
	x->rn_b = -1 - b;
	if (isnormal)
		x->rn_flags |= RNF_NORMAL;
	return (x);
}

static int	/* XXX: arbitrary ordering for non-contiguous masks */
rn1_lexobetter( void *m_arg, void *n_arg)
{
	u_char *mp = (u_char*)m_arg, *np = (u_char*)n_arg, *lim;

	if (*mp > *np)
		return 1;  /* not really, but need to check longer one first */
	if (*mp == *np) 
		for (lim = mp + *mp; mp < lim;)
			if (*mp++ > *np++)
				return 1;
	return 0;
}

static struct usn_radix_mask *
rn1_new_radix_mask( struct usn_radix_node *tt, struct usn_radix_mask *next)
{
	struct usn_radix_mask *m;

	MKGet(m);
	if (m == 0) {
		DEBUG("Mask for route not entered\n");
		return (0);
	}
	Bzero(m, sizeof *m);
	m->rm_b = tt->rn_b;
	m->rm_flags = tt->rn_flags;
	if (tt->rn_flags & RNF_NORMAL)
		m->rm_leaf = tt;
	else
		m->rm_mask = tt->rn_mask;
	m->rm_mklist = next;
	tt->rn_mklist = m;
	return m;
}

struct usn_radix_node *
rn1_addroute( void *v_arg, void *n_arg, struct usn_radix_node_head *head, struct usn_radix_node treenodes[2])
{
	caddr_t v = (caddr_t)v_arg, netmask = (caddr_t)n_arg;
	struct usn_radix_node *t, *x = 0, *tt;
	struct usn_radix_node *saved_tt, *top = head->rnh_treetop;
	short b = 0, b_leaf = 0;
	int keyduplicated;
	caddr_t mmask;
	struct usn_radix_mask *m, **mp;
   DEBUG("add a route");
	/*
	 * In dealing with non-contiguous masks, there may be
	 * many different routes which have the same mask.
	 * We will find it useful to have a unique pointer to
	 * the mask to speed avoiding duplicate references at
	 * nodes and possibly save time in calculating indices.
	 */
	if (netmask)  {
		if ((x = rn1_addmask(netmask, 0, top->rn_off)) == 0)
			return (0);
		b_leaf = x->rn_b;
		b = -1 - x->rn_b;
		netmask = x->rn_key;
	}
	/*
	 * Deal with duplicated keys: attach node to previous instance
	 */
	saved_tt = tt = rn1_insert(v, head, &keyduplicated, treenodes);
	if (keyduplicated) {
		for (t = tt; tt; t = tt, tt = tt->rn_dupedkey) {
			if (tt->rn_mask == netmask)
				return (0);
			if (netmask == 0 ||
			    (tt->rn_mask &&
			     ((b_leaf < tt->rn_b) || /* index(netmask) > node */
			       rn1_refines(netmask, tt->rn_mask) ||
			       rn1_lexobetter(netmask, tt->rn_mask))))
				break;
		}
		/*
		 * If the mask is not duplicated, we wouldn't
		 * find it among possible duplicate key entries
		 * anyway, so the above test doesn't hurt.
		 *
		 * We sort the masks for a duplicated key the same way as
		 * in a masklist -- most specific to least specific.
		 * This may require the unfortunate nuisance of relocating
		 * the head of the list.
		 *
		 * We also reverse, or doubly link the list through the
		 * parent pointer.
		 */
		if (tt == saved_tt) {
			struct	usn_radix_node *xx = x;
			/* link in at head of list */
			(tt = treenodes)->rn_dupedkey = t;
			tt->rn_flags = t->rn_flags;
			tt->rn_p = x = t->rn_p;
			t->rn_p = tt;
			if (x->rn_l == t) x->rn_l = tt; else x->rn_r = tt;
			saved_tt = tt; x = xx;
		} else {
			(tt = treenodes)->rn_dupedkey = t->rn_dupedkey;
			t->rn_dupedkey = tt;
			tt->rn_p = t;
			if (tt->rn_dupedkey)
				tt->rn_dupedkey->rn_p = tt;
		}
#ifdef RN_DEBUG
		t=tt+1; tt->rn_info = rn_nodenum++; t->rn_info = rn_nodenum++;
		tt->rn_twin = t; tt->rn_ybro = rn_clist; rn_clist = tt;
#endif
		tt->rn_key = (caddr_t) v;
		tt->rn_b = -1;
		tt->rn_flags = RNF_ACTIVE;
	}
	/*
	 * Put mask in tree.
	 */
	if (netmask) {
		tt->rn_mask = netmask;
		tt->rn_b = x->rn_b;
		tt->rn_flags |= x->rn_flags & RNF_NORMAL;
	}
	t = saved_tt->rn_p;
	if (keyduplicated)
		goto on2;
	b_leaf = -1 - t->rn_b;
	if (t->rn_r == saved_tt) 
      x = t->rn_l; 
   else 
      x = t->rn_r;
	/* Promote general routes from below */
	if (x->rn_b < 0) { 
	    for (mp = &t->rn_mklist; x; x = x->rn_dupedkey)
		if (x->rn_mask && (x->rn_b >= b_leaf) && x->rn_mklist == 0) {
			*mp = m = rn1_new_radix_mask(x, 0);
			if (m)
				mp = &m->rm_mklist;
		}
	} else if (x->rn_mklist) {
		/*
		 * Skip over masks whose index is > that of new node
		 */
		for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist)
			if (m->rm_b >= b_leaf)
				break;
		t->rn_mklist = m; *mp = 0;
	}
on2:
	/* Add new route to highest possible ancestor's list */
	if ((netmask == 0) || (b > t->rn_b ))
		return tt; /* can't lift at all */
	b_leaf = tt->rn_b;
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	/*
	 * Search through routes associated with node to
	 * insert new route according to index.
	 * Need same criteria as when sorting dupedkeys to avoid
	 * double loop on deletion.
	 */
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist) {
		if (m->rm_b < b_leaf)
			continue;
		if (m->rm_b > b_leaf)
			break;
		if (m->rm_flags & RNF_NORMAL) {
			mmask = m->rm_leaf->rn_mask;
			if (tt->rn_flags & RNF_NORMAL) {
				DEBUG("Non-unique normal route, mask not entered");
				return tt;
			}
		} else
			mmask = m->rm_mask;
		if (mmask == netmask) {
			m->rm_refs++;
			tt->rn_mklist = m;
			return tt;
		}
		if (rn1_refines(netmask, mmask) || rn1_lexobetter(netmask, mmask))
			break;
	}
	*mp = rn1_new_radix_mask(tt, *mp);
	return tt;
}

struct usn_radix_node *
rn1_delete( void *v_arg, void *netmask_arg, struct usn_radix_node_head *head)
{
	struct usn_radix_node *t, *p, *x, *tt;
	struct usn_radix_mask *m, *saved_m, **mp;
	struct usn_radix_node *dupedkey, *saved_tt, *top;
	caddr_t v, netmask;
	int b, head_off, vlen;

	v = (caddr_t)v_arg;
	netmask = (caddr_t)netmask_arg;
	x = head->rnh_treetop;
	tt = rn1_search(v, x);
	head_off = x->rn_off;
	vlen =  *(u_char *)v;
	saved_tt = tt;
	top = x;
	if (tt == 0 ||
	    Bcmp(v + head_off, tt->rn_key + head_off, vlen - head_off))
		return (0);
	/*
	 * Delete our route from mask lists.
	 */
	if (netmask) {
		if ((x = rn1_addmask(netmask, 1, head_off)) == 0)
			return (0);
		netmask = x->rn_key;
		while (tt->rn_mask != netmask)
			if ((tt = tt->rn_dupedkey) == 0)
				return (0);
	}
	if (tt->rn_mask == 0 || (saved_m = m = tt->rn_mklist) == 0)
		goto on1;
	if (tt->rn_flags & RNF_NORMAL) {
		if (m->rm_leaf != tt || m->rm_refs > 0) {
			DEBUG( "rn_delete: inconsistent annotation\n");
			return 0;  /* dangling ref could cause disaster */
		}
	} else { 
		if (m->rm_mask != tt->rn_mask) {
			DEBUG( "rn_delete: inconsistent annotation\n");
			goto on1;
		}
		if (--m->rm_refs >= 0)
			goto on1;
	}
	b = -1 - tt->rn_b;
	t = saved_tt->rn_p;
	if (b > t->rn_b)
		goto on1; /* Wasn't lifted at all */
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != top);
	for (mp = &x->rn_mklist; (m = *mp); mp = &m->rm_mklist)
		if (m == saved_m) {
			*mp = m->rm_mklist;
			MKFree(m);
			break;
		}
	if (m == 0) {
		DEBUG( "rn_delete: couldn't find our annotation\n");
		if (tt->rn_flags & RNF_NORMAL)
			return (0); /* Dangling ref to us */
	}
on1:
	/*
	 * Eliminate us from tree
	 */
	if (tt->rn_flags & RNF_ROOT)
		return (0);
#ifdef RN_DEBUG
	/* Get us out of the creation list */
	for (t = rn_clist; t && t->rn_ybro != tt; t = t->rn_ybro) {}
	if (t) t->rn_ybro = tt->rn_ybro;
#endif
	t = tt->rn_p;
	dupedkey = saved_tt->rn_dupedkey;
	if (dupedkey) {
		/*
		 * Here, tt is the deletion target, and
		 * saved_tt is the head of the dupedkey chain.
		 */
		if (tt == saved_tt) {
			x = dupedkey; x->rn_p = t;
			if (t->rn_l == tt) t->rn_l = x; else t->rn_r = x;
		} else {
			/* find node in front of tt on the chain */
			for (x = p = saved_tt; p && p->rn_dupedkey != tt;)
				p = p->rn_dupedkey;
			if (p) {
				p->rn_dupedkey = tt->rn_dupedkey;
				if (tt->rn_dupedkey)
					tt->rn_dupedkey->rn_p = p;
			} else DEBUG( "rn_delete: couldn't find us\n");
		}
		t = tt + 1;
		if  (t->rn_flags & RNF_ACTIVE) {
#ifndef RN_DEBUG
			*++x = *t; p = t->rn_p;
#else
			b = t->rn_info; *++x = *t; t->rn_info = b; p = t->rn_p;
#endif
			if (p->rn_l == t) p->rn_l = x; else p->rn_r = x;
			x->rn_l->rn_p = x; x->rn_r->rn_p = x;
		}
		goto out;
	}
	if (t->rn_l == tt) x = t->rn_r; else x = t->rn_l;
	p = t->rn_p;
	if (p->rn_r == t) p->rn_r = x; else p->rn_l = x;
	x->rn_p = p;
	/*
	 * Demote routes attached to us.
	 */
	if (t->rn_mklist) {
		if (x->rn_b >= 0) {
			for (mp = &x->rn_mklist; (m = *mp);)
				mp = &m->rm_mklist;
			*mp = t->rn_mklist;
		} else {
			/* If there are any key,mask pairs in a sibling
			   duped-key chain, some subset will appear sorted
			   in the same order attached to our mklist */
			for (m = t->rn_mklist; m && x; x = x->rn_dupedkey)
				if (m == x->rn_mklist) {
					struct usn_radix_mask *mm = m->rm_mklist;
					x->rn_mklist = 0;
					if (--(m->rm_refs) < 0)
						MKFree(m);
					m = mm;
				}
			if (m)
				DEBUG( "%s %p at %p\n",
					    "rn_delete: Orphaned Mask", m, x);
		}
	}
	/*
	 * We may be holding an active internal node in the tree.
	 */
	x = tt + 1;
	if (t != x) {
#ifndef RN_DEBUG
		*t = *x;
#else
		b = t->rn_info; *t = *x; t->rn_info = b;
#endif
		t->rn_l->rn_p = t; t->rn_r->rn_p = t;
		p = x->rn_p;
		if (p->rn_l == x) p->rn_l = t; else p->rn_r = t;
	}
out:
	tt->rn_flags &= ~RNF_ACTIVE;
	tt[1].rn_flags &= ~RNF_ACTIVE;
	return (tt);
}

int
rn1_walktree(struct usn_radix_node_head *h, int (*f)(struct usn_radix_node*, void*), void *w)
{
	int error;
	struct usn_radix_node *base, *next;
	struct usn_radix_node *rn = h->rnh_treetop;
	/*
	 * This gets complicated because we may delete the node
	 * while applying the function f to it, so we need to calculate
	 * the successor node in advance.
	 */
	/* First time through node, go left */
   DEBUG("walktree");
	while (rn->rn_b >= 0)
		rn = rn->rn_l;
	for (;;) {
		base = rn;
		/* If at right child go back up, otherwise, go right */
		while (rn->rn_p->rn_r == rn && (rn->rn_flags & RNF_ROOT) == 0)
			rn = rn->rn_p;
		/* Find the next *leaf* since next node might vanish, too */
		for (rn = rn->rn_p->rn_r; rn->rn_b >= 0;)
			rn = rn->rn_l;
		next = rn;
		/* Process leaves */
      rn = base;
		while (rn) {
		//while (rn = base) {
			base = rn->rn_dupedkey;
			if (!(rn->rn_flags & RNF_ROOT) && (error = (*f)(rn, w)))
				return (error);
         rn = base;
		}
		rn = next;
		if (rn->rn_flags & RNF_ROOT)
			return (0);
	}
	/* NOTREACHED */
}


int
rn1_inithead( void **head, int off)
{
	struct usn_radix_node_head *rnh;
	struct usn_radix_node *t, *tt, *ttt;
	if (*head)
		return (1);
	R_Malloc(rnh, struct usn_radix_node_head *, sizeof (*rnh));
	if (rnh == 0) {
      DEBUG("rn1_inithead: malloc failed");
		return (0);
   }
	Bzero(rnh, sizeof (*rnh));
	*head = rnh;
	t = rn1_newpair(rn_zeros, off, rnh->rnh_nodes);
	ttt = rnh->rnh_nodes + 2;
	t->rn_r = ttt;
	t->rn_p = t;
	tt = t->rn_l;
	tt->rn_flags = t->rn_flags = RNF_ROOT | RNF_ACTIVE;
	tt->rn_b = -1 - off;
	*ttt = *tt;
	ttt->rn_key = rn_ones;
	rnh->rnh_addaddr = rn1_addroute;
	rnh->rnh_deladdr = rn1_delete;
	rnh->rnh_matchaddr = rn1_match;
	rnh->rnh_lookup = rn1_lookup;
	rnh->rnh_walktree = rn1_walktree;
	rnh->rnh_treetop = t;
	return (1);
}

void
rn1_init()
{
	char *cp, *cplim;
   DEBUG("rn1_init: start");
/*
#ifdef KERNEL
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_maxrtkey > g_max_keylen)
			g_max_keylen = dom->dom_maxrtkey;
#endif
	if (g_max_keylen == 0) {
		DEBUG( "rn_init: radix functions require max_keylen be set\n");
		return;
	}
*/
   // Fixing maximum len of rtkey by using inet domain.
   g_max_keylen = sizeof(struct usn_sockaddr_in);
   DEBUG("g_max_keylen=%d", g_max_keylen);
	R_Malloc(rn_zeros, char *, 3 * g_max_keylen);
	if (rn_zeros == NULL) {
		DEBUG("rn1_init failed");
      return;
   }
	Bzero(rn_zeros, 3 * g_max_keylen);
	rn_ones = cp = rn_zeros + g_max_keylen;
	addmask_key = cplim = rn_ones + g_max_keylen;
	while (cp < cplim)
		*cp++ = -1;
	if (rn1_inithead((void **)&mask_rnhead, 0) == 0)
		DEBUG("rn_init 2");
   DEBUG("rn1_init: done");
}



