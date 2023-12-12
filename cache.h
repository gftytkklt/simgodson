/*
 * cache.h - cache module interfaces
 *
 * This file is a part of the SimpleScalar tool suite, written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 * The file has been substantially modified by Doug Burger, as a
 * part of the Galileo research project.  Alain Kagi has also 
 * contributed to this code.
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997 by Todd M. Austin
 *
 * Distributed as prt of sim-alpha release
 * Copyright (C) 1999 by Raj Desikan
 * This source file is distributed "as is" in the hope that it will be
 * useful.  The tool set comes with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use. 
 * 
 * Everyone is granted permission to copy, modify and redistribute
 * this tool set under the following conditions:
 * 
 *    This source code is distributed for non-commercial use only. 
 *    Please contact the maintainer for restrictions applying to 
 *    commercial use.
 *
 *    Permission is granted to anyone to make or distribute copies
 *    of this source code, either as received or modified, in any
 *    medium, provided that all copyright notices, permission and
 *    nonwarranty notices are preserved, and that the distributor
 *    grants the recipient permission for further redistribution as
 *    permitted by this document.
 *
 *    Permission is granted to distribute this file in compiled
 *    or executable form under the same conditions that apply for
 *    source code, provided that either:
 *
 *    A. it is accompanied by the corresponding machine-readable
 *       source code,
 *    B. it is accompanied by a written offer, with no time limit,
 *       to give anyone a machine-readable copy of the corresponding
 *       source code in return for reimbursement of the cost of
 *       distribution.  This written offer must permit verbatim
 *       duplication by anyone, or
 *    C. it is distributed by someone who received only the
 *       executable form, and is accompanied by a copy of the
 *       written offer of source code that they received concurrently.
 *
 * In other words, you are welcome to use, share and improve this
 * source file.  You are forbidden to forbid anyone else to use, share
 * and improve what you give them.
 *
 *
 * Substantially modified by Doug Burger, also incorporates modifications
 * originally made by Alain Kagi.
 *
 *
 *
 */

#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include "host.h"
#include "misc.h"
#include "mips.h"
#include "mem.h"
#include "stats.h"
//#include "issue.h"

/*
 * This module contains code to implement various cache-like structures.  The
 * user instantiates caches using cache_new().  When instantiated, the user
 * may specify the geometry of the cache (i.e., number of set, line size,
 * associativity), and supply a block access function.  The block access
 * function indicates the latency to access lines when the cache misses,
 * accounting for any component of miss latency, e.g., bus acquire latency,
 * bus transfer latency, memory access latency, etc...  In addition, the user
 * may allocate the cache with or without lines allocated in the cache.
 * Caches without tags are useful when implementing structures that map data
 * other than the address space, e.g., TLBs which map the virtual address
 * space to physical page address, or BTBs which map text addresses to
 * branch prediction state.  Tags are always allocated.  User data may also be
 * optionally attached to cache lines, this space is useful to storing
 * auxilliary or additional cache line information, such as predecode data,
 * physical page address information, etc...
 *
 * The caches implemented by this module provide efficient storage management
 * and fast access for all cache geometries.  When sets become highly
 * associative, a hash table (indexed by address) is allocated for each set
 * in the cache.
 *
 * This module also tracks latency of accessing the data cache, each cache has
 * a hit latency defined when instantiated, miss latency is returned by the
 * cache's block access function, the caches may service any number of hits
 * under any number of misses, the calling simulator should limit the number
 * of outstanding misses or the number of hits under misses as per the
 * limitations of the particular microarchitecture being simulated.
 *
 */

/* highly associative caches are implemented using a hash table lookup to
   speed block access, this macro decides if a cache is "highly associative" */
#define CACHE_HIGHLY_ASSOC(cp)	((cp)->assoc > 4)

/* cache replacement policy */
enum cache_policy {
  LRU,		/* way list order: MRU -> LRU */
  Random,	/* way list order: arbitrary */
  FIFO,		/* way list order: oldest block -> newest block */
  LRF           /* way list order: Least recently filled */
};

/* Cache addressing scheme */
enum cache_trans {
  VIVT,		/* Virtually indexed, virtually tagged */
  PIVT,		/* Physically indexed, virtually tagged */
  VIPT,		/* Virtually indexed, physically tagged */
  PIPT,		/* Physically indexed, physically tagged */
  NONE		/* Translation is irrelevant (for TLBs) */
};

/*These flags are used to set the is_a_tlb field in the cache struct to indicate if it is a cache or a TLB*/

#define		A_TLB	0x0001
#define		A_CACHE	0x0002
#define		IS_A_TLB(q)	((q) & A_TLB)
#define		IS_A_CACHE(q)	((q) & A_CACHE)


#define		CACHE_MISS	0
#define		TLB_MISS	-1
#define		BAD_ADDRESS	-2
#define		MSHRS_FULL	-3
#define		TARGET_FULL	-4
#define		MISS_PENDING	-5
#define     TARGET_OCCUPIED -6

#define		CACHE_HIT(x)	(x > 0)

/* Define flags that connote the different types of memory/fetch stalls */
#define CACHE_STALL    	0x01
#define BRANCH_STALL 	0x02
#define TLB_STALL 		0x04
#define MSHR_STALL 		0x08
#define SAMPLE_STALL 	0x10
#define REFILL_STALL 	0x20

/* Define major cache data structures */
struct cache_blk
{
  struct cache_blk *way_next;	/* next block in the MRU chain */
  struct cache_blk *way_prev;	/* previous block in the MRU chain */
  struct cache_blk *hash_next;	/* next block in the hash bucket chain, only
								   used in highly-associative caches */
  /* since hash table lists are typically small, there is no previous
     pointer, deletion requires a trip through the hash table bucket list */

  md_addr_t tag;		/* data block tag value */
  md_addr_t set;		/* saved set index (needed for VIPT caches) */
                        /* (Could combine tag & pset for space->time tradeoff) */
#define MAX_CPUS 16
  unsigned int data_directory[MAX_CPUS];
  unsigned int inst_directory[MAX_CPUS];
  
  unsigned int sb_valid;	/* Valid bits for subblocks */
  unsigned int sb_used;		/* Used bits for subblocks */
  unsigned int sb_dirty;	/* Dirty bits for subblocks */
  unsigned int status;		/* block status */
  unsigned int dirty;		/* add for directory protocol. indicates whether L2 cache data is identical to memory.*/
  tick_t ready;				/* when will this block be accessable */

  struct block_state *extra_state; /* "permanent" extra state field for this block */
  char *user_data;		/* pointer to user defined data, e.g.,
				   		   pre-decode data or physical page address */
  /* DATA should be pointer-aligned due to preceeding field */
  /* NOTE: this is a variable-size tail array, this must be the LAST field
     defined in this structure! */
  char data[1];			/* actual data block starts here, block size
				   		   should probably be a multiple of 8 */
  int blk_no;           /* Block number for the line predictor */
};

/* Maximum number of caches and off-cache resources that can be defined */
#define MAX_NUM_CACHES    100
#define MAX_NUM_RESOURCES 100

/* block status values */
#define CACHE_BLK_VALID		0x00000001	/* block in valid, in use */
#define CACHE_BLK_DIRTY		0x00000002	/* dirty block */
#define CACHE_BLK_TAG		0x00000004	/* tag for prefetch */

/* cache line states */
enum {UNTOUCHED = 0, INVALID = 1, SHARED = 2, EXCLUSIVE = 3 , DIRTY = 4,DIRTY_INVALID = 5,MODIFIED = 7, MODIFIED_ABOVE};

/* request and response types */
enum {NO_REQ = 0, READ_SHD = 1, READ_EX = 2, UPGRADE = 3, WRITEBACK = 4, ELIMINATE = 5, INTERVENTION_SHD = 6, INTERVENTION_EXC = 7, INVALIDATE = 8, ACK = 9, ACK_DATA = 10, NACK = 11};

struct cache_set
{
  struct cache_blk **hash;	/* hash table: for fast access w/assoc, NULL
				  			   for low-assoc caches */
  struct cache_blk *way_head;	/* head of way list */
  struct cache_blk *way_tail;	/* tail of way list */
  struct cache_blk *blks;	/* cache blocks, allocated sequentially, so
				 			   this pointer can also be used for random
				 			   access to cache blocks */
};

struct cache
{
  /* parameters */
  char *name;			/* cache name */
  int nsets;			/* number of sets */
  int bsize;			/* block size in bytes */
  int sbsize;			/* subblock size in bytes */
  int balloc;			/* maintain cache contents? */
  int usize;			/* user allocated data size */
  int assoc;			/* cache associativity */
  enum cache_policy policy;	/* cache replacement policy */
  enum cache_trans trans;	/* cache translation/addressing policy */
  unsigned int hit_latency;	/* cache hit latency */
  
  void *owner;			/* belongs to which cpu */
  
  /* if set to 1 then this cache is really a TLB 
   * else this is a cache*/
  unsigned int is_a_tlb;	

  int num_resources;
  int resource_code;
  enum resource_type resource_type[MAX_NUM_RESOURCES];
  char *resource_names[MAX_NUM_RESOURCES];
  void *resources[MAX_NUM_RESOURCES];

  struct cache *tlb;

  /* DERIVED data, for fast decoding */
  int hsize;			/* cache set hash table size */
  md_addr_t blk_mask;
  int set_shift;
  md_addr_t set_mask;	/* use *after* shift */
  int tag_shift;
  md_addr_t tag_mask;	/* use *after* shift */
  md_addr_t tagset_mask;	/* used for fast hit detection */
  
  md_addr_t inst_mask;    /* Used by line predictor */
  int inst_shift;         /* Amount to shift */
  int subblock_ratio;		/* number of transfer blocks per address block */
  int subblock_vector_length;	/* size of a valid bit vector, in bytes */
  int subblock_mask;		/* use *after* shift */
  int subblock_shift;		/* used for extracting subblock index */

  counter_t hits;            	/* hits to all blocks */
  counter_t misses;         	/* misses to address blocks */
  counter_t partial_misses;  	/* misses to invalid subblocks within a mapped block */
  counter_t replacements;		/* replacements of valid blocks */
  counter_t invalidations;
  counter_t writebacks;		/* Total number of writebacks */

  counter_t read_traffic;  	/* Number of bytes read into cache */
  counter_t write_traffic; 	/* Number of bytes written back (or through) from the cache */
  counter_t address_traffic;  	/* Number of bytes transmitted for addresses per cache */
  /* prefetching cache */
  counter_t mshr_full;        /* Number of times mshr was full */
  int prefetch;

  /* prefetch stats */
  counter_t prefetch_issued;	/* */
  counter_t prefetch_out_of_bound;/* prefetch illegal addr */
  counter_t prefetch_in_cache;	/* blk already in the cache */
  counter_t prefetch_requested;	/* blk already requested */
  counter_t prefetch_full;	/* no mshr entry available */
  counter_t prefetch_crosses_page_boundary; /* next blk on different page */
  
  /* Way predictor */
  int way_pred_latency;          
  int ***way_pred_table;
  
  /* line predictor */
  int line_predictor;
  
  struct line_pred_struct {
    md_addr_t next_addr;
    int line_pred_hist;
  } ***line_pred_table;
  counter_t line_pred_hits;
  counter_t line_pred_misses;
  counter_t line_pred_lookups;
  
  counter_t way_pred_lookups;
  counter_t way_pred_hits;
  counter_t way_pred_misses;
  
  /* vctim buffer */
  int victim_buffer;
  /* non blocking cache */
#include "mshr.h"

  /* last block to hit, used to optimize cache hit processing */
  md_addr_t last_tagset;	/* tag of last line accessed */
  struct cache_blk *last_blk;	/* cache block last accessed */

  /* replace block */
  md_addr_t replace_paddr;
  int replace_status;

  /* refilling block */
  md_addr_t refill_set;               /* set of current refilling block */
  int refill_way;               /* way of current refilling block */
  int refill_bitmap;            /* bitmap of current refilling block */
  struct cache_blk *refill_blk;
  int refill_new_status;		/* new status of cache block when refill finished. */
  
  /* data blocks */
  char *data;			/* pointer to data blocks allocation */

  /* NOTE: this is a variable-size tail array, this must be the LAST field
     defined in this structure! */
  struct cache_set sets[1];	/* each entry is a set */
};

/* Create buffer structure to hold cache accesses that occur on TLB misses */
typedef struct _cache_access_packet
{
  struct cache *cp;		/* cache to access */
  unsigned int cmd;		/* access type, Read or Write */
  md_addr_t addr;		/* address of access */
  enum trans_cmd vorp;		/* is the address virtual or physical*/
  int nbytes;			/* number of bytes to access */
  void *obj;			/* Pointer to needed object */
  RELEASE_FN_TYPE release_fn;	/* Function to call upon cache release */
  VALID_FN_TYPE valid_fn;	/* Function to check validity of return */
  MSHR_STAMP_TYPE stamp;	/* Tag for MSHR */
  struct _cache_access_packet *next; /* Pointer to next entry in list */
} cache_access_packet;

#define CACHE_ACCESS_PACKET_FREE_LIST 64

/* Queue for buffered cache events that are blocked due to full mshrs */

struct mshr_full_event {
  cache_access_packet *c_packet;
  struct mshr_full_event *next;
};

/* create and initialize a general cache structure, see "struct cache"
   definition for a description of parameters */
struct cache *					/* pointer to cache created */
cache_func_create(char *name,			/* name of the cache */
		  int nsets,			/* total number of sets in cache */
		  int bsize,			/* block (line) size of cache */
		  int subblock,			/* subblock factor of cache lines */
		  int balloc,			/* allocate data space for blocks? */
		  int usize,			/* size of user data to alloc w/blks */
		  int assoc,			/* associativity of cache */
		  unsigned int hit_latency,	/* latency in cycles for a hit */
		  enum cache_policy policy,	/* replacement policy w/in sets */
		  enum cache_trans trans,	/* translation policy  */
		  int prefetch,			/* Turn on prefetching or not */
		  int resources, 		/* Number of buses below cache */
		  int resource_code,		/* Bus selection algorithm index */
		  char *res_names[]);		/* Names of connected buses */

/* create and initialize a general cache structure, see "struct cache"
   definition for a description of parameters */
struct cache *					/* pointer to cache created */
cache_timing_create(char *name,			/* name of the cache */
		    int nsets,			/* total number of sets in cache */
		    int bsize,			/* block (line) size of cache */
		    int subblock,		/* subblock factor of cache lines */
		    int balloc,			/* allocate data space for blocks? */
		    int usize,			/* size of user data to alloc w/blks */
		    int assoc,			/* associativity of cache */
		    unsigned int hit_latency,	/* latency in cycles for a hit */
		    enum cache_policy policy,	/* replacement policy w/in sets */
		    enum cache_trans trans,	/* translation policy  */
		    int prefetch,		/* Turn on prefetching or not */
		    int resources, 		/* Number of buses below cache */
		    int resource_code,		/* Bus selection algorithm index */
		    char *res_names[]);		/* Names of connected buses */
		    

/* parse policy */
enum cache_policy cache_char2policy(char c);

/* parse translationy */
enum cache_trans cache_string2trans(char *s);

enum list_loc_t { Head, Tail };

/* print cache configuration */
void
cache_config(struct cache *cp,		/* cache instance */
	     FILE *stream);		/* output stream */

/* register cache stats */
void
cache_func_reg_stats(struct cache *cp,	/* cache instance */
		     struct stat_sdb_t *sdb);/* stats database */

/* register cache stats */
void
cache_timing_reg_stats(struct cache *cp,	/* cache instance */
		       struct stat_sdb_t *sdb);/* stats database */

/* access a cache, perform a CMD operation on cache CP at address ADDR,
   places NBYTES of data at *P, returns latency of operation if initiated
   at NOW, places pointer to block user data in *UDATA, *P is untouched if
   cache blocks are not allocated (!CP->BALLOC), UDATA should be NULL if no
   user data is attached to blocks */
int						/* latency of access in cycles */
cache_timing_access(tick_t now,			/* time of access */
		    cache_access_packet *c_packet); 	/* Packet containing cache access arguments */

int						/* latency of access in cycles */
cache_func_access(tick_t now,			/* time of access */
		  cache_access_packet *c_packet); 	/* Packet containing cache access arguments */

/* Schedules a blocked cache access to be restarted at a given time */
void schedule_restart_cache_access(tick_t when, cache_access_packet *c_packet,
				   MSHR_STAMP_TYPE stamp);

/* cache access functions, these are safe, they check alignment and
   permissions */
#define cache_double(cp, cmd, addr, p, now, udata)	\
  cache_access(cp, cmd, addr, p, sizeof(double), now, udata)
#define cache_float(cp, cmd, addr, p, now, udata)	\
  cache_access(cp, cmd, addr, p, sizeof(float), now, udata)
#define cache_dword(cp, cmd, addr, p, now, udata)	\
  cache_access(cp, cmd, addr, p, sizeof(long long), now, udata)
#define cache_word(cp, cmd, addr, p, now, udata)	\
  cache_access(cp, cmd, addr, p, sizeof(int), now, udata)
#define cache_half(cp, cmd, addr, p, now, udata)	\
  cache_access(cp, cmd, addr, p, sizeof(short), now, udata)
#define cache_byte(cp, cmd, addr, p, now, udata)	\
  cache_access(cp, cmd, addr, p, sizeof(char), now, udata)

/* cache access macros */
#define CACHE_TAG(cp, addr)	((addr) >> (cp)->tag_shift)
#define CACHE_SET(cp, addr)	(((addr) >> (cp)->set_shift) & (cp)->set_mask)
#define CACHE_BLOCKID(cp, addr) (addr >> (cp->set_shift))

#define CACHE_SET_INC(cp, set)	((set + 1) & (cp)->set_mask)
#define CACHE_BLK(cp, addr)	((addr) & (cp)->blk_mask)
#define CACHE_TAGSET(cp, addr)	((addr) & (cp)->tagset_mask)
#define CACHE_INST_OFFSET(cp, addr) ((addr)>>(cp)->inst_shift & (cp)->inst_mask)

/* subblock helper macros */
#define CACHE_SB_TAG(cp, x)		((x >> cp->subblock_shift) & cp->subblock_mask)
#define CACHE_SET_SB_BIT(sb, vector)	((vector) |= (1 << (sb)))
#define CACHE_GET_SB_BIT(sb, vector)	((vector) & (1 << (sb)))

#define COUNT_VALID_SB_BITS(cp, blk, x)				\
        ({int i; x = 0; 					\
          for (i = 0; i < cp->subblock_mask; i++)		\
   	    {x += (CACHE_GET_SB_BIT(i, blk->sb_valid) != 0);}}, x)

#define COUNT_DIRTY_SB_BITS(cp, blk, x)				\
        ({int i; x = 0; 					\
          for (i = 0; i < cp->subblock_mask; i++)		\
   	    {x += (CACHE_GET_SB_BIT(i, blk->sb_dirty) != 0);}}, x)

/* --- subblock action macros, change these to extend cache/sb functionality */

#define CREATE_SUBBLOCK_FETCH_VECTOR(cp, blk, sb)	\
        (1 << sb)

#define FETCH_SUBBLOCK(cp, blk, addr)			\
        (cp->subblock_ratio > 1)

#define IS_CACHE_SUBBLOCKED(cp)				\
        (cp->subblock_ratio > 1)

#define IS_BLOCK_SUBBLOCKED(blk)			\
        (blk->sb_valid != 0)				

#define ARE_SUBBLOCKS_DIRTY(blk)			\
        (blk->sb_dirty != 0)				

#define IS_SUBBLOCK_VALID(blk, sb)  (CACHE_GET_SB_BIT(sb, blk->sb_valid))

/* --- End of subblock action macros */

/* extract/reconstruct a block address */
#define CACHE_BADDR(cp, addr)	((addr) & ~(cp)->blk_mask)
#define CACHE_SBADDR(cp, addr)	((addr) & ~((cp->sbsize)-1))

#define CACHE_MK_BADDR(cp, tag, set)					\
  (((tag) << (cp)->tag_shift)|((set) << (cp)->set_shift))
#define CACHE_MK_BTAG(cp, tag, set) (CACHE_BLOCKID(cp, (CACHE_MK_BADDR(cp, tag, set))))

/* index an array of cache blocks, non-trivial due to variable length blocks */
#define CACHE_BINDEX(cp, blks, i)					\
  ((struct cache_blk *)(((char *)(blks)) +				\
			(i)*(sizeof(struct cache_blk))))

/* cache data block accessor, type parameterized */
#define __CACHE_ACCESS(type, data, bofs)				\
  (*((type *)(((char *)data) + (bofs))))

/* cache data block accessors, by type */
#define CACHE_DOUBLE(data, bofs)  __CACHE_ACCESS(double, data, bofs)
#define CACHE_FLOAT(data, bofs)	  __CACHE_ACCESS(float, data, bofs)
#define CACHE_WORD(data, bofs)	  __CACHE_ACCESS(unsigned int, data, bofs)
#define CACHE_HALF(data, bofs)	  __CACHE_ACCESS(unsigned short, data, bofs)
#define CACHE_BYTE(data, bofs)	  __CACHE_ACCESS(unsigned char, data, bofs)

/* cache block hashing macros, this macro is used to index into a cache
   set hash table (to find the correct block on N in an N-way cache), the
   cache set index function is CACHE_SET, defined above */
#define CACHE_HASH(cp, key)						\
  (((key >> 24) ^ (key >> 16) ^ (key >> 8) ^ key) & ((cp)->hsize-1))

/* copy data out of a cache block to buffer indicated by argument pointer p */
#define CACHE_BCOPY(cmd, blk, bofs, p, nbytes)	\
  if (!is_write(cmd))							\
    {									\
      switch (nbytes) {							\
      case 1:								\
	*((unsigned char *)p) = CACHE_BYTE(&blk->data[0], bofs); break;	\
      case 2:								\
	*((unsigned short *)p) = CACHE_HALF(&blk->data[0], bofs); break;\
      case 4:								\
	*((unsigned int *)p) = CACHE_WORD(&blk->data[0], bofs); break;	\
      default:								\
	{ /* >= 8, power of two, fits in block */			\
	  int words = nbytes >> 2;					\
	  while (words-- > 0)						\
	    {								\
	      *((unsigned int *)p) = CACHE_WORD(&blk->data[0], bofs);	\
	      p += 4; bofs += 4;					\
	    }\
	}\
      }\
    }\
  else /* cmd == Write */						\
    {									\
      switch (nbytes) {							\
      case 1:								\
	CACHE_BYTE(&blk->data[0], bofs) = *((unsigned char *)p); break;	\
      case 2:								\
        CACHE_HALF(&blk->data[0], bofs) = *((unsigned short *)p); break;\
      case 4:								\
	CACHE_WORD(&blk->data[0], bofs) = *((unsigned int *)p); break;	\
      default:								\
	{ /* >= 8, power of two, fits in block */			\
	  int words = nbytes >> 2;					\
	  while (words-- > 0)						\
	    {								\
	      CACHE_WORD(&blk->data[0], bofs) = *((unsigned int *)p);	\
	      p += 4; bofs += 4;					\
	    }\
	}\
    }\
  }

/* attempts to find cache BLK in SET matching TAG, if found goto HIT_LABEL,
   fall through otherwise.  Method: linear search */
#define FIND_BLK_MATCH(CP, BLK, SET, TAG, SB, DID_SB_MISS, HIT_LABEL)	\
	for ((BLK) = (CP)->sets[(SET)].way_head;			\
	     (BLK);							\
	     (BLK) = (BLK)->way_next)	{				\
              if ((BLK)->tag == (TAG) && ((BLK)->status & CACHE_BLK_VALID)) {	\
	     if (IS_BLOCK_SUBBLOCKED((BLK)) && (!IS_SUBBLOCK_VALID((BLK), (SB)))) \
                DID_SB_MISS = 1; \
	     else \
	     goto HIT_LABEL ; }}  	


#define FIND_ADDR_MATCH(BLK, TAG, SET) \
        for((BLK) = vbuf.way_head; \
            (BLK);\
	    (BLK) = (BLK)->way_next) { \
              if ((BLK)->tag == TAG && (BLK)->set == SET) \
                break; \
            }

/* return non-zero if block containing address ADDR is contained in cache
   CP, this interface is used primarily for debugging and asserting cache
   invariants */
struct cache_blk*			/* non-zero if access would hit */
cache_probe(struct cache *cp,		/* cache instance to probe */
		int is_store,
	    md_addr_t addr,		/* virtual address of block to probe */
	    md_addr_t paddr,	/* physical address of block to probe */
		int* request);

/* query itlb for match of ADDR, if hit return paddr in PADDR
 */
int                                     /* non-zero if access would hit */
itlb_probe(int cpuid,md_addr_t addr,md_addr_t *paddr);

/* query dtlb for match of ADDR, if hit return paddr in PADDR
 */
int                                     /* non-zero if access would hit */
dtlb_probe(int cpuid,md_addr_t addr,md_addr_t *paddr);

/* flush the entire cache, returns latency of the operation */
unsigned int					/* latency of the flush operation */
cache_func_flush(struct cache *cp,		/* cache instance to flush */
		 tick_t now);		/* time of cache flush */

/* flush the entire cache, returns latency of the operation */
unsigned int					/* latency of the flush operation */
cache_timing_flush(struct cache *cp,		/* cache instance to flush */
		   tick_t now);		/* time of cache flush */

/* flush the block containing ADDR from the cache CP, returns the latency of
   the block flush operation */
unsigned int					/* latency of flush operation */
cache_func_flush_addr(struct cache *cp,		/* cache instance to flush */
		      md_addr_t addr,	/* address of block to flush */
		      tick_t now);	/* time of cache flush */

/* flush the block containing ADDR from the cache CP, returns the latency of
   the block flush operation */
unsigned int					/* latency of flush operation */
cache_timing_flush_addr(struct cache *cp,	/* cache instance to flush */
			md_addr_t addr,	/* address of block to flush */
			tick_t now);	/* time of cache flush */

/* Prototypes from cache_common.c */

void unlink_htab_ent(struct cache *cp, struct cache_set *set, struct cache_blk *blk);
void update_way_list(struct cache_set *set, struct cache_blk *blk, enum list_loc_t where);
unsigned int count_valid_bits(struct cache *cp, unsigned int vector);
enum cache_policy cache_char2policy(char c);
enum cache_trans cache_string2trans(char *s);
void cache_config(struct cache *cp, FILE *stream);
struct cache_blk * find_blk_match_no_jump(struct cache *cp, md_addr_t set, md_addr_t tag);
void *cache_follow(struct cache *cp, md_addr_t addr, enum resource_type *type);
void increase_cache_packet_free_list();
struct cache_blk *
replace_block(struct cache *cp,int bindex, md_addr_t paddr,int req,int inst,int intervention, int new_status);
/* call when last refill is seen to mark the refilling block valid*/
void mark_valid(struct cache *cp, int mode);
void mark_bitmap(struct cache *cp,int b);
void cache_free_access_packet(cache_access_packet *buf);
void link_htab_ent(struct cache *cp, struct cache_set *set, struct cache_blk *blk);
cache_access_packet *
cache_create_access_packet(void *mp,
			   unsigned int cmd,
			   md_addr_t addr, 
			   enum trans_cmd vorp,
			   int nbytes, 
			   void *obj, 
			   RELEASE_FN_TYPE release_fn,
			   VALID_FN_TYPE valid_fn, 
			   MSHR_STAMP_TYPE stamp);


void cache_unblock_memop(tick_t when, cache_access_packet *c_packet);
void schedule_unblock_memop(tick_t when, unsigned int ptr, 
			    MSHR_STAMP_TYPE null_stamp);

/* returns true if a hit, false if a miss, to be used when fast forwarding */
int
icache_extremely_fast_access (struct cache * cp,	/* cache to access */
			      md_addr_t addr);	/* address of access */
/* returns true if a hit, false if a miss, to be used when fast forwarding */
int
dl1_extremely_fast_access (int type,	/* access type, Read or Write */
			   md_addr_t addr);	/* address of access */
int
dtlb_extremely_fast_access (struct cache *cp,	/* access type, Read or Write */
                            md_addr_t addr);    /* address */

/* Variables defined in cache.c */
extern int regular_mshrs;
extern int prefetch_mshrs;
extern int mshr_targets;
extern char *dcache_name;
extern char *icache_name;
extern char *cache_dl2_name;
extern int cache_nelt;
extern char *cache_configs[MAX_NUM_CACHES];
extern int num_caches;
extern struct cache *caches[MAX_NUM_CACHES];
extern struct cache *icache;
extern struct cache *dcache;
extern struct cache *cache_dl2;  //added for power.c
extern struct cache *cache_il2;  //added for leakage.c
extern int flush_on_syscalls;
extern int way_pred_latency;
extern int victim_buf_ent;
extern int victim_buf_lat;
extern struct cache_set vbuf;
extern counter_t victim_buf_hits;
extern counter_t victim_buf_misses;
extern int prefetch_dist; 
extern int cache_target_trap;
extern int cache_diff_addr_trap;
extern int cache_mshrfull_trap;
extern counter_t cache_quadword_trap;
extern counter_t cache_diffaddr_trap;
extern int perfectl2;

#endif /* CACHE_H */
