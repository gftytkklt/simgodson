/*
 * cache_common.c - cache module routines for functional simulation
 *
 * This file is a part of the SimpleScalar tool suite, originally
 * written by Todd M. Austin as a part of the Multiscalar Research 
 * Project. The file has been rewritten by Doug Burger, as a
 * part of the Galileo research project.  Alain Kagi has also 
 * contributed to this code.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997 by Todd M. Austin
 * This file is also part of the Alpha simulator tool suite written by
 * Raj Desikan as part of the Bullseye project.
 * Copyright (C) 1999 by Raj Desikan
 *
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "memory.h"
#include "cache.h"
#include "eventq.h"
#include "misc.h"
#include "tlb.h"
#include "bus.h"

#include "godson2_cpu.h"
#include "cache2mem.h"
#include "noc.h"
#include "sim.h"

/* Head to list of unused mshr_full_event entries */
struct mshr_full_event *mshr_full_free_list;

/* Set predefined (but changeable via command-line) maximal
   numbers of various mshr components */
int regular_mshrs;
int prefetch_mshrs;
int mshr_targets;

/* flush caches on system calls */
int flush_on_syscalls;

/* Name of level-one data cache {name|none} */
char *dcache_name;

/* Name of level-one instruction cache {name|none} */
char *icache_name;

/* Name of level-two unified cache {name|none} */
char *cache_dl2_name;

/* Cache definition counter */
int cache_nelt = 0;

/* Array of cache strings */
char *cache_configs[MAX_NUM_CACHES];

int num_caches;

/* Array of cache pointers */
struct cache *caches[MAX_NUM_CACHES];

/* pointer to level one instruction cache */
struct cache *icache = NULL;

/* pointer to level one data cache */
struct cache *dcache = NULL;

/* pointer to level two unified cache */
struct cache *cache_dl2 = NULL;

/* pointer to level two unified cache */
struct cache *cache_il2 = NULL;

/* Head to list of unused cache buffer entries */
cache_access_packet *cache_packet_free_list;


/* way predictor latency */
int way_pred_latency;

/* victim buffer */
struct cache_set vbuf;

/* number of entries */
int victim_buf_ent;

/* hit latency */
int victim_buf_lat;

/* Number of victim buffer hits */
counter_t victim_buf_hits;

/* Number of victim buffer misses */
counter_t victim_buf_misses;

/* Number of blocks to prefetch on a icache miss */
int prefetch_dist;

/* Whether to trap on loads to same MSHR targets */
int cache_target_trap;

/* Whether to trap on loads where different addresses map to same cache line */
int cache_diff_addr_trap;

/* Whether to trap if mshrs are full */
int cache_mshrfull_trap;

/* Number of MSHR target traps */
counter_t cache_quadword_trap;

/* Number of diff address same cache line traps */
counter_t cache_diffaddr_trap;

/* If simulating perfect L2 */
int perfectl2;

/* Here's a general function version of the above two macros */
unsigned int 
count_valid_bits(struct cache *cp, unsigned int vector)
{
  int i, num = 0;

  for (i = 0; i < cp->subblock_ratio; i++)
    num += (CACHE_GET_SB_BIT(i, vector) != 0);
  return(num);
}

/* unlink BLK from the hash table bucket chain in SET */
void
unlink_htab_ent(struct cache *cp,		/* cache to update */
		struct cache_set *set,		/* set containing bkt chain */
		struct cache_blk *blk)		/* block to unlink */
{
  struct cache_blk *prev, *ent;
  int index = CACHE_HASH(cp, blk->tag);

  /* locate the block in the hash table bucket chain */
  for (prev=NULL,ent=set->hash[index];
       ent;
       prev=ent,ent=ent->hash_next)
    {
      if (ent == blk)
	break;
    }
  assert(ent);
  
  /* unlink the block from the hash table bucket chain */
  if (!prev)
    {
      /* head of hash bucket list */
      set->hash[index] = ent->hash_next;
    }
  else
    {
      /* middle or end of hash bucket list */
      prev->hash_next = ent->hash_next;
    }
  ent->hash_next = NULL;
}

/* insert BLK onto the head of the hash table bucket chain in SET */
void
link_htab_ent(struct cache *cp,		/* cache to update */
	      struct cache_set *set,	/* set containing bkt chain */
	      struct cache_blk *blk)	/* block to insert */
{
  int index = CACHE_HASH(cp, blk->tag);

  /* insert block onto the head of the bucket chain */
  blk->hash_next = set->hash[index];
  set->hash[index] = blk;
}

/* insert BLK into the order way chain in SET at location WHERE */
void
update_way_list(struct cache_set *set,	/* set contained way chain */
		struct cache_blk *blk,	/* block to insert */
		enum list_loc_t where)	/* insert location */
{
  /* unlink entry from the way list */
  if (!blk->way_prev && !blk->way_next)
    {
      /* only one entry in list (direct-mapped), no action */
      assert(set->way_head == blk && set->way_tail == blk);
      /* Head/Tail order already */
      return;
    }
  /* else, more than one element in the list */
  else if (!blk->way_prev)
    {
      assert(set->way_head == blk && set->way_tail != blk);
      if (where == Head)
	{
	  /* already there */
	  return;
	}
      /* else, move to tail */
      set->way_head = blk->way_next;
      blk->way_next->way_prev = NULL;
    }
  else if (!blk->way_next)
    {
      /* end of list (and not front of list) */
      assert(set->way_head != blk && set->way_tail == blk);
      if (where == Tail)
	{
	  /* already there */
	  return;
	}
      set->way_tail = blk->way_prev;
      blk->way_prev->way_next = NULL;
    }
  else
    {
      /* middle of list (and not front or end of list) */
      assert(set->way_head != blk && set->way_tail != blk);
      blk->way_prev->way_next = blk->way_next;
      blk->way_next->way_prev = blk->way_prev;
    }

  /* link BLK back into the list */
  if (where == Head)
    {
      /* link to the head of the way list */
      blk->way_next = set->way_head;
      blk->way_prev = NULL;
      set->way_head->way_prev = blk;
      set->way_head = blk;
    }
  else if (where == Tail)
    {
      /* link to the tail of the way list */
      blk->way_prev = set->way_tail;
      blk->way_next = NULL;
      set->way_tail->way_next = blk;
      set->way_tail = blk;
    }
  else
    panic("bogus WHERE designator");
}

/* parse policy */
enum cache_policy			/* replacement policy enum */
cache_char2policy(char c)		/* replacement policy as a char */
{
  switch (c) {
  case 'l': return LRU;
  case 'r': return Random;
  case 'f': return FIFO;
  case 'F': return LRF;
  default: fatal("bogus replacement policy, `%c'", c);
  }
}

/* parse policy */
enum cache_trans			/* replacement policy enum */
cache_string2trans(char *s)
{
  if (!mystricmp(s, "vivt"))
    return VIVT;
  else if (!mystricmp(s, "vipt"))
    return VIPT;
  else if (!mystricmp(s, "pipt"))
    return PIPT;
  else if (!mystricmp(s, "pivt"))
    fatal("Physically indexed, virtually tagged caches not supported!\n");
  else
    fatal("Unrecognized cache translation policy specified (VIVT, VIPT, PIPT are legal\n");
  return PIPT;	/* Default, should never happen */
}

/* print cache configuration */
void
cache_config(struct cache *cp,		/* cache instance */
	     FILE *stream)		/* output stream */
{
  fprintf(stream,
	  "cache %s: %d size, %d sets, %d byte blocks\n",
	  cp->name, cp->assoc * cp->nsets * cp->bsize, cp->nsets, cp->bsize);
  fprintf(stream,
	  "cache %s: %d-way, %s replacement policy, write-back\n",
	  cp->name, cp->assoc,
	  cp->policy == LRU ? "LRU"
	  : cp->policy == Random ? "Random"
	  : cp->policy == FIFO ? "FIFO"
          : cp->policy == LRF ? "LRF"
	  : (abort(), ""));
  if (cp->prefetch)
    fprintf(stream, "cache: %s: prefetching...\n", cp->name);
  else
    fprintf(stream, "cache: %s: no prefetch\n", cp->name);
}

struct cache_blk *
find_blk_match_no_jump(struct cache *cp,
		       md_addr_t set,
		       md_addr_t tag)
{
  struct cache_blk *repl;

  if (!cp->hsize)
    {
      for (repl = cp->sets[set].way_head; repl; repl=repl->way_next)
	{
	  if ((repl->tag == tag) && (repl->status & CACHE_BLK_VALID))
	    break;
	}
    }
  else
    {
      int hindex = CACHE_HASH(cp, tag);

      for (repl=cp->sets[set].hash[hindex]; repl; repl=repl->hash_next)
	{
	  if (repl->tag == tag && (repl->status & CACHE_BLK_VALID))
	    break;
	}
    }
  return(repl);
}

void *
cache_follow(struct cache *cp, 
             md_addr_t addr, 
             enum resource_type *type)
{
  int index = 0;
  void *next_cache;

  if (cp->num_resources > 1)
    {
      switch(cp->resource_code) {
      case 0:
	break;
      default:    
	break;
      }
    }
  *type = cp->resource_type[index];
  next_cache = (void *)cp->resources[index];

  if (*type == Bus)
    {
      struct bus *bus = (struct bus *)cp->resources[index];
      *type = bus->resource_type[index];
      assert(*type != Bus);
      next_cache = (void *)bus->resources[index];
    }

  return next_cache;
}

/* Allocates CACHE_ACCESS_PACKET_FREE_LIST entries of type cache_access_packet
   onto the appropriate free list */
void increase_cache_packet_free_list()
{
  int i;
  cache_access_packet *temp;
  static int free_flag = 0;

  if (free_flag)
    fprintf(stderr, "Warning: Increasing cache_packet_free_list\n");

  free_flag++;
  cache_packet_free_list = NULL;

  for (i=0; i<CACHE_ACCESS_PACKET_FREE_LIST; i++)
    {
      temp = (cache_access_packet *) malloc(sizeof(cache_access_packet));
      if (temp == NULL)
	fatal("Malloc on cache access packet free list expansion failed");
      temp->next = cache_packet_free_list;
      cache_packet_free_list = temp;
    }
}

/* Create a "cache access packet", which is used to hold the transient information
   for a specific access to a specific cache.  Makes blocking the request (and 
   subsequently restarting) much cleaner. Could differentiate between functional
   and timing versions by not initializing the last three fields in the functional
   version (more efficient), but that's probably not a lot of overhead.  */
cache_access_packet *
cache_create_access_packet(void *mp, 			/* Pointer to level in the memory hierarchy */
			   unsigned int cmd,
			   md_addr_t addr, 
			   enum trans_cmd vorp,
			   int nbytes, 
			   void *obj, 
			   RELEASE_FN_TYPE release_fn,
			   VALID_FN_TYPE valid_fn, 
			   MSHR_STAMP_TYPE stamp)
{
  cache_access_packet *temp;

  /* If free list is exhausted, add some more entries */
  if (!cache_packet_free_list)
    {
      increase_cache_packet_free_list();
    }

  temp = cache_packet_free_list;
  cache_packet_free_list = cache_packet_free_list->next;

  /* Initialize fields of buffering function */
  temp->cp = mp;
  temp->cmd = cmd;
  temp->addr = addr;
  temp->vorp = vorp;
  temp->nbytes = nbytes;
  temp->obj = obj;
  temp->release_fn = release_fn;
  temp->valid_fn = valid_fn;
  temp->stamp = stamp;

  return(temp);
}

void
cache_free_access_packet(cache_access_packet *buf)
{
  buf->next = cache_packet_free_list;
  cache_packet_free_list = buf;
}

/* return non-zero if block containing address ADDR is contained in cache
   CP, this interface is used primarily for debugging and asserting cache
   invariants. */
/* TODO:presently VIPT only */
struct cache_blk*			/* non-zero if access would hit */
cache_probe(struct cache *cp,		/* cache instance to probe */
	    int is_store,
	    md_addr_t addr,		/* vaddress of block to probe */
	    md_addr_t paddr,	/* paddress of block to probe */
	    int *request)
{
  md_addr_t tag = CACHE_TAG(cp, paddr);
  md_addr_t set = CACHE_SET(cp, addr);
  //md_addr_t sb = CACHE_SB_TAG(cp, addr);
  int b = (addr & 0x1f) >> 3;
  struct cache_blk *blk;
  struct godson2_cpu *st = cp->owner;

  *request = NO_REQ;

  /* permissions are checked on cache misses */

  /* TODO: fast hit */

#if 0
  if (cp->hsize)
    {
      /* higly-associativity cache, access through the per-set hash tables */
      int hindex = CACHE_HASH(cp, tag);

      for (blk=cp->sets[set].hash[hindex];
	   blk;
	   blk=blk->hash_next)
	{	
	  if ((blk->tag == tag && (blk->status & CACHE_BLK_VALID)) &&
	      ((!IS_BLOCK_SUBBLOCKED(blk)) || 
	       (CACHE_GET_SB_BIT(sb, blk->sb_valid))))
	    return blk;
	}
    }
  else
#endif
    {
      if(cp == st->dcache)  st->L1_accesses++;
      if(is_store)  st->L1_stores++;

      /* low-associativity cache, linear search the way list */
      for (blk = cp->sets[set].way_head; blk; blk = blk->way_next) {

	if (blk->tag == tag && blk->status != INVALID && blk->status != DIRTY_INVALID){

	  /* tag match with _valid_ state, so now handle specific states */
	  switch (blk->status) {

	  case MODIFIED:
	    /* hit in L1 for both loads and stores */
	    if(cp == st->dcache)  st->L1_hits++;
	    break;

	  case EXCLUSIVE:
	    /* hit in L1 for loads and stores */
	    if(cp == st->dcache)  st->L1_hits++;
	    /* but for stores, change L1 and L2 states to modified */
	    if (is_store) {
	      st->L1_exclusive_to_modified_changes++;
	      blk->status = MODIFIED;
	      /*if (L2_states[pid][L2_line] == MODIFIED
		|| L2_states[pid][L2_line] == EXCLUSIVE)
		SET_L2_STATE (pid, L2_line, MODIFIED_ABOVE);*/
	    }
	    break;

	  case SHARED:
	    if (! is_store){ 
	      if(cp == st->dcache)  st->L1_hits++; /* simple hit for reads */
	    }else{
	      /* writes requires an upgrade request */
	      if(cp == st->dcache){
			st->L1_upgrade_requests++;
			st->L1_misses++;
	      }
	      *request = UPGRADE;
	      blk = NULL;  /* still write miss */
	    }
	    break;
	  }

	  return blk;
	}
      }
    }

  /* cache block not found */
  *request = is_store ? READ_EX : READ_SHD;  /* write or read miss */

  /* hit refilling parts */
  if (cp->refill_blk!=NULL && cp->refill_set == set && 
      cp->refill_blk->tag == tag && (cp->refill_bitmap & (1<<b))){
    if (cp == st->dcache){
      if (cp->refill_new_status == SHARED && is_store){
		*request = UPGRADE;
		return NULL;
      }
      else{
		return cp->refill_blk;
      }
    }else
      return cp->refill_blk;
  }


  if(is_store) {  
    st->L1_read_exclusive_requests++;
  } else {
    st->L1_read_shared_requests++;
  }

  /* now treat L1_misses as dcache miss */
  if(cp == st->dcache)  st->L1_misses++;

  return NULL;
}

extern int is_splash;

/* query itlb for match of ADDR, if hit return paddr in PADDR
 */
int                                     /* non-zero if access would hit */
itlb_probe(int cpuid,md_addr_t addr,md_addr_t *paddr)
{
  /* TODO */
  if(is_splash){
    *paddr = addr;
  }else{
    *paddr = addr + cpuid * NEW_THREAD_TOTAL_SIZE; 
  }

  return TRUE;
}

/* query dtlb for match of ADDR, if hit return paddr in PADDR
 */
int                                     /* non-zero if access would hit */
dtlb_probe(int cpuid,md_addr_t addr,md_addr_t *paddr)
{
  /* TODO */
  if(is_splash){
    *paddr = addr;
  }else{
    *paddr = addr + cpuid * NEW_THREAD_TOTAL_SIZE;
  }
  
  return TRUE;
}

/* if bindex==-1,randomly choosen a block to replace
 * otherwise replace given block */
struct cache_blk *
replace_block(struct cache *cp,int bindex, md_addr_t paddr,int req,int inst,int intervention, int new_status)
{
  extern tick_t sim_cycle;
  struct cache_blk *repl, *blk;

  md_addr_t tag, set;

  tag = CACHE_TAG(cp,paddr);
  set = CACHE_SET(cp,paddr);

  if (bindex==-1) {
    bindex = (int)sim_cycle & (cp->assoc - 1);
    /* avoid replace last refilled way */
    if (set == cp->refill_set) {
      bindex = cp->assoc - 1 - cp->refill_way;
    }
  }


  repl = CACHE_BINDEX(cp, cp->sets[set].blks, bindex);

  /* if this occurs, there must be a ack for upgrade request */
  for (blk = cp->sets[set].way_head; blk; blk = blk->way_next) {
    if (blk->tag == tag && blk->status == SHARED){
      repl = blk;
    }
  }
	  
#if 0
  /* remove this block from the hash bucket chain, if hash exists */
  if (cp->hsize)
    unlink_htab_ent(cp, &cp->sets[set], repl);

  /* Update victim buffer */
  if (cp->victim_buffer) {
    vbuf.way_tail->tag = repl->tag;
    vbuf.way_tail->set = repl->set;
    update_way_list(&vbuf, vbuf.way_tail, Head);
  }      
#endif

  /* when doing replace , early refill must have been completed. */
  assert(repl->status != DIRTY_INVALID);
  /* write back replaced block data */
  /* if we are doing intervention, we don't care whether we can find a matched block, the only thing we want to do is probe L1 cache and return its status. the paddr that is used to replace_block must be identical to the paddr that resides in wtbkq. */
  int rep_status;
  md_addr_t rep_paddr = 0;
  if (intervention){
    struct godson2_cpu *st = cp->owner;
    /* one cycle ealier than real hardware */
    if (repl->tag == tag)
      wtbkq_enter(st,st->cpuid,ELIMINATE,paddr,inst,repl->status,intervention);
    else
      wtbkq_enter(st,st->cpuid,ELIMINATE,paddr,inst,INVALID,intervention);
  } else if (repl->status != INVALID) {
    struct godson2_cpu *st = cp->owner;
    st->L1_writebacks++;
    /* one cycle ealier than real hardware */
    rep_paddr = ((repl->tag)<<cp->tag_shift) + (paddr & ((1<<cp->tag_shift) - 1));
    rep_paddr &= cp->tagset_mask;
    if (repl->status == MODIFIED){
      wtbkq_enter(st,st->cpuid,WRITEBACK,rep_paddr,inst,repl->status,intervention);
    }else{
      wtbkq_enter(st,st->cpuid,ELIMINATE,rep_paddr,inst,repl->status,intervention);
    }
  } else{
    struct godson2_cpu *st = cp->owner;
    st->wtbkq_num --;
    assert(st->wtbkq_num >= 0);
    eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, WTBK, HOME);
  }

  rep_status = repl->status;
  
  /* update block tags */
  if (!intervention || (intervention && repl->tag == tag)) {
    repl->tag = tag;
    repl->set = set;
    repl->status = INVALID; /*CACHE_BLK_VALID*/;
    repl->sb_valid = repl->sb_dirty = 0;
  }

  if (!intervention){
    /* remember current refilling block */
    cp->replace_paddr = rep_paddr;
    cp->replace_status = rep_status;
    cp->refill_set = set;
    cp->refill_way = bindex;
    cp->refill_blk = repl;
    cp->refill_bitmap = 0;
    if (req != NO_REQ) {
#ifdef MESI
	if (!inst && req == READ_SHD)
	  cp->refill_new_status = new_status;
	else
	  cp->refill_new_status = (req == READ_SHD) ? SHARED : EXCLUSIVE;
#else
      cp->refill_new_status = (req == READ_SHD) ? SHARED : EXCLUSIVE;
#endif
	}
  }

  /* Blow away last tagset so we don't fake a hit */
  cp->last_tagset = 0xffffffff;

#if 0
  /* link this entry back into the hash table */
  if (cp->hsize)
    link_htab_ent(cp, &cp->sets[set], repl);
#endif

  return repl;
}

/* call when last refill is seen to mark the refilling block valid*/
void mark_valid(struct cache *cp, int mode)
{
  cp->refill_blk->status = (cp->refill_blk->status & DIRTY) | mode;
}

void mark_bitmap(struct cache *cp,int b)
{
  cp->refill_bitmap |= (1<<b);
}

/* returns true if a hit, false if a miss, to be used when fast forwarding */
int
icache_extremely_fast_access (struct cache * cp,	/* cache to access */
			      md_addr_t addr)	/* address of access */
{
  md_addr_t tag = CACHE_TAG (cp, addr);
  md_addr_t set = CACHE_SET (cp, addr);
  struct cache_blk *blk, *repl;
  int bindex;


  /* No check for fast hit - checked by caller */

  /* low-associativity cache, linear search the way list */
  for (blk = cp->sets[set].way_head; blk; blk = blk->way_next)
    {
      if (blk->tag == tag && (blk->status & CACHE_BLK_VALID))
	{
	  return TRUE;
	}
    }


  /* cache block not found */

  /* random replacement */
  bindex = (int)sim_cycle & (cp->assoc - 1);
  /* avoid replace last refilled way */
  if (set == cp->refill_set) {
    bindex = cp->assoc - 1 - cp->refill_way;
  }
  repl = CACHE_BINDEX(cp, cp->sets[set].blks, bindex);

  /* update block tags */
  repl->tag = tag;
  repl->status = CACHE_BLK_VALID;	/* dirty bit set on update */

  cp->refill_set = set;
  cp->refill_way = bindex;
  cp->refill_blk = repl;

  return FALSE;
}

/* returns true if a hit, false if a miss, to be used when fast forwarding */
int
dl1_extremely_fast_access (int cmd,	/* access type, Read or Write */
			   md_addr_t addr)	/* address of access */
{
  struct cache *cp = dcache;
  md_addr_t tag = CACHE_TAG (cp, addr);
  md_addr_t set = CACHE_SET (cp, addr);
  struct cache_blk *blk, *repl;
  int bindex;

  /* low-associativity cache, linear search the way list */
  for (blk = cp->sets[set].way_head; blk; blk = blk->way_next)
    {
      if (blk->tag == tag && (blk->status & CACHE_BLK_VALID))
	{
	  /* update dirty status */
	  if (cmd == Write)
	    blk->status |= CACHE_BLK_DIRTY;

	  /* tag is unchanged, so hash links (if they exist) are still valid */

	  return TRUE;
	}
    }

  /* cache block not found */

  /* random replacement */
  bindex = (int)sim_cycle & (cp->assoc - 1);
  /* avoid replace last refilled way */
  if (set == cp->refill_set) {
    bindex = cp->assoc - 1 - cp->refill_way;
  }
  repl = CACHE_BINDEX(cp, cp->sets[set].blks, bindex);

  /* update block tags */
  repl->tag = tag;
  repl->status = CACHE_BLK_VALID;	/* dirty bit set on update */

  cp->refill_set = set;
  cp->refill_way = bindex;
  cp->refill_blk = repl;

  /* update dirty status */
  if (cmd == Write)
    repl->status |= CACHE_BLK_DIRTY;

  return FALSE;
}

int
dtlb_extremely_fast_access (struct cache *cp,	/* access type, Read or Write */
                            md_addr_t addr)     /* address */
{
  /* TODO */
  return 0;
}


