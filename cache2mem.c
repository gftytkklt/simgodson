/*
 * cache2mem.c - missq/writeback queue implementation
 *
 * This file is part of the godson2 simulator tool suite.
 *
 * Copyright (C) 2004 by Fuxin Zhang, ICT.
 *
 * This source file is distributed "as is" in the hope that it will be
 * useful.  It is distributed with no warranty, and no author or
 * distributor accepts any responsibility for the consequences of its
 * use. 
 *
 * Everyone is granted permission to copy, modify and redistribute
 * this source file under the following conditions:
 *
 *    This tool set is distributed for non-commercial use only. 
 *    Please contact the maintainer for restrictions applying to 
 *    commercial use of these tools.
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
#include <math.h>
#include "mips.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "cache.h"
#include "bpred.h"
#include "tlb.h"
#include "eventq.h"
//#include "fetch.h"
//#include "decode.h"
//#include "issue.h"
//#include "writeback.h"
//#include "map.h"
#include "stats.h"
#include "syscall.h"
#include "sim.h"
#include "godson2_cpu.h"
//#include "lsq.h"
#include "cache2mem.h"
#include "noc.h"
#include "ptrace.h"

// int mem_read_first_delay;
// int mem_read_interval_delay;
// int mem_write_delay;

// int memq_ifq_size;

/* Initialize the refill free list */
static void refill_init_free_list(struct godson2_cpu *st)
{
  int i;
  struct refill_packet *temp;

  st->refill_free_list = NULL;
  warn("Increasing refill free list size");

  for (i=0; i<REFILL_FREE_LIST_SIZE; i++){
    temp = (struct refill_packet *) 
      malloc(sizeof(struct refill_packet));

    if(!temp) 
      fatal("out of virtual memory");
    temp->next = st->refill_free_list;
    st->refill_free_list = temp;
  }
}  

/* get an entry from free list */
struct refill_packet *
refill_get_from_free_list(struct godson2_cpu *st)
{
  struct refill_packet *temp;

  /* should not be exhausted */ 
  assert(st->refill_free_list);

  temp = st->refill_free_list;
  st->refill_free_list = st->refill_free_list->next;
  return (temp);
}

/* return an inst to the free list */
void refill_return_to_free_list(struct godson2_cpu *st,struct refill_packet *temp){
  temp->next = st->refill_free_list;
  st->refill_free_list = temp;
}

/* Initialize the refill free list */
static void memq_init_free_list(struct godson2_cpu *st)
{
  int i;
  struct memory_queue *temp;
  st->memq_free_list = NULL;
  warn("Increasing memq free list size");
  for (i=0; i<memq_ifq_size; i++){
    temp = (struct memory_queue *) 
      calloc(1,sizeof(struct memory_queue));

    if(!temp) 
      fatal("out of virtual memory");
    temp->next = st->memq_free_list;
    st->memq_free_list = temp;
  }
}  

/* get an entry from free list */
static inline struct memory_queue *
memq_get_from_free_list(struct godson2_cpu *st)
{
  struct memory_queue *temp;

  /* should not be exhausted */ 
  assert(st->memq_free_list);

  temp = st->memq_free_list;
  st->memq_free_list = st->memq_free_list->next;

  st->memq_num++;

  return (temp);
}

/* return an inst to the free list */

static inline void memq_return_to_free_list(struct godson2_cpu *st, struct memory_queue *temp)
{
  temp->next = st->memq_free_list;
  st->memq_free_list = temp;
  st->memq_num--;
}

/* type==0: is read
 * type==1: is write
 */
static int memq_enter(struct godson2_cpu *st, int type, int index)
{
  struct memory_queue *tmp;

  if (st->memq_num >= memq_ifq_size) return 0;

  tmp = memq_get_from_free_list(st);

  tmp->read = type;
  tmp->qid = index;
  tmp->count = 0;

  /* add to memq */
  tmp->next = st->memq_head;
  st->memq_head = tmp;
  st->memq_access++;

  return 1;
}


/* intervention shared special cpu dcache.return ACK or ACK_DATA. */
int intervention_shared(struct godson2_cpu *st, md_addr_t paddr,struct cache_blk** pblk,int* cache_status)
{
#if 0 // wtbkq should be checked when external request is entering
  int i;
  for(i=0 ; i<wtbkq_ifq_size ; i++){
    if(BLOCK_MATCH(st->wtbkq[i].paddr, paddr) && st->wtbkq[i].w){
      st->wtbkq[i].valid = 0;
      st->wtbkq[i].w = 0;
    }
  }
#endif

  md_addr_t tag = CACHE_TAG(st->dcache, paddr);
  md_addr_t set = CACHE_SET(st->dcache, paddr);
  struct cache_blk *blk;

  *pblk = NULL;
  int ack = ACK;
  *cache_status = INVALID;

  for (blk = st->dcache->sets[set].way_head; blk; blk = blk->way_next) {
	if (blk->tag == tag && blk->status != INVALID){
	  *cache_status = blk->status;
	  if (blk->status == MODIFIED)
		ack = ACK_DATA;
	  blk->status = SHARED;
	  *pblk = blk;
	  break;
	}
  }

  st->L1_external_intervention_shareds++;
  return ack;
}

/* invalidate special cpu dcache*/
int intervention_exclusive(struct godson2_cpu *st, md_addr_t paddr, struct cache_blk** pblk,int* cache_status)
{
  md_addr_t tag = CACHE_TAG(st->dcache, paddr);
  md_addr_t set = CACHE_SET(st->dcache, paddr);
  struct cache_blk *blk;
  *pblk = NULL;
  int ack = ACK;
  *cache_status = INVALID;

  for (blk = st->dcache->sets[set].way_head; blk; blk = blk->way_next) {
	if (blk->tag == tag && blk->status != INVALID){
	  *cache_status = blk->status;
	  if (blk->status == MODIFIED)
		ack = ACK_DATA;
	  blk->status = INVALID;
	  *pblk = blk;
	  break;
	}
  }
  st->L1_external_intervention_exclusives++;
  return ack;
}

/* invalidate special cpu dcache*/
void invalidate(struct godson2_cpu *st, md_addr_t paddr,struct cache_blk** pblk,int* cache_status)
{
  md_addr_t tag = CACHE_TAG(st->dcache, paddr);
  md_addr_t set = CACHE_SET(st->dcache, paddr);
  struct cache_blk *blk;
  *pblk = NULL;
  *cache_status = INVALID;

  for (blk = st->dcache->sets[set].way_head; blk; blk = blk->way_next) {
    if (blk->tag == tag && blk->status != INVALID){
      assert(blk->status != EXCLUSIVE && blk->status != MODIFIED);
      *cache_status = blk->status;
      blk->status = INVALID;
      *pblk = blk;
      break;
    }
  }

  st->L1_external_invalidations++;
}


int L2_probe_delay = 4;
int L2_write_delay = 4;

/* L1 miss, so missq probe L2 cache and maintain cache coherence */
void scache_probe(tick_t now, int home_cpuid, int req, int qid, int /*md_addr_t*/ paddr)
{
  struct godson2_cpu * st = &cpus[home_cpuid];
  int request_cpuid = st->missq[qid].cpuid;/* the request initiator's cpuid */
  md_addr_t tag = CACHE_TAG(st->cache_dl2,paddr);
  md_addr_t set = CACHE_SET(st->cache_dl2,paddr);
  struct cache_blk *blk, *repl;
  int i;

  int is_inst = st->missq[qid].inst;

  for (blk = st->cache_dl2->sets[set].way_head; blk; blk = blk->way_next) {

	if (blk->tag == tag && blk->status != INVALID){

	  switch (blk->status) {

		case EXCLUSIVE:
		  switch(req){
			case READ_SHD:/* modified or exclusive block receive shared read request, need to send intervention_shd request to other L1 */
			  if((!is_inst && blk->data_directory[request_cpuid]) || 
				  (is_inst && blk->inst_directory[request_cpuid])){
				/* dcache state is not coherent with scache directory, wait for directory state changing */
				st->missq[qid].wait_for_response = 1;
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
			  }else{
#ifdef MESI
				int dir_count = 0;
#endif
				missq_change_state(st,qid,MQ_MODIFY_L1);
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
				for(i=0 ; i<total_cpus ; i++){
				  st->missq[qid].data_directory[i] = (i==request_cpuid) ? 0 : blk->data_directory[i];
#ifdef MESI
				  dir_count += st->missq[qid].data_directory[i];
#endif
				  st->missq[qid].inst_directory[i] = 0;
				  st->missq[qid].data_intervention_sent[i] = 0;
				  st->missq[qid].data_ack_received[i] = 0;
				  st->missq[qid].inst_intervention_sent[i] = 0;
				  st->missq[qid].inst_ack_received[i] = 0;
				}
#ifdef MESI
				if (st->missq[qid].inst)
				  st->missq[qid].cache_status = SHARED;
				else {
				  st->missq[qid].cache_status = (dir_count == 0 ? EXCLUSIVE : SHARED);
				}
#endif
				st->missq[qid].intervention_type = INTERVENTION_SHD;
				st->missq[qid].wait_for_response = 0;
				/* record L2 block which state and directory need to be changed when refill */
				st->missq[qid].L2_changed_blk = blk;
			  }
			  break;

			case READ_EX:/* modified or exclusive block receive exclusive read request, need to send invalidate request to other L1 */

			  assert(!is_inst);
			  if(blk->data_directory[request_cpuid]){
				/* dcache state is not coherent with scache directory, wait for directory state changing */
				st->missq[qid].wait_for_response = 1;
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
			  }else{
				missq_change_state(st,qid,MQ_MODIFY_L1);
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
				for(i=0 ; i<total_cpus ; i++){
				  st->missq[qid].data_directory[i] = (i==request_cpuid) ? 0 : blk->data_directory[i];
				  st->missq[qid].inst_directory[i] = 0;
				  st->missq[qid].data_intervention_sent[i] = 0;
				  st->missq[qid].data_ack_received[i] = 0;
				  st->missq[qid].inst_intervention_sent[i] = 0;
				  st->missq[qid].inst_ack_received[i] = 0;
				}
				st->missq[qid].intervention_type = INTERVENTION_EXC;
				st->missq[qid].wait_for_response = 0;
				/* record L2 block which state and directory need to be changed when refill */
				st->missq[qid].L2_changed_blk = blk;
			  }
			  break;

			case UPGRADE:/* modified or exclusive block receive upgrade request, this occurs when a upgrade request issued after a read shared request but directory first receive upgrade request, need to send invalidate request to other L1 */
			  assert(!is_inst);
			  if(blk->data_directory[request_cpuid]){
				/* dcache state is not coherent with scache directory, wait for directory state changing */
				st->missq[qid].wait_for_response = 1;
				if (st->missq[qid].read_local_l2);
				  st->missq[qid].read_local_l2 = 0;
			  }else{
				missq_change_state(st,qid,MQ_MODIFY_L1);
				if (st->missq[qid].read_local_l2);
				  st->missq[qid].read_local_l2 = 0;
				for(i=0 ; i<total_cpus ; i++){
				  st->missq[qid].data_directory[i] = (i==request_cpuid) ? 0 : blk->data_directory[i];
				  st->missq[qid].inst_directory[i] = 0;
				  st->missq[qid].data_intervention_sent[i] = 0;
				  st->missq[qid].data_ack_received[i] = 0;
				  st->missq[qid].inst_intervention_sent[i] = 0;
				  st->missq[qid].inst_ack_received[i] = 0;
				}
				st->missq[qid].intervention_type = INTERVENTION_EXC;

				st->missq[qid].wait_for_response = 0;
				/* record L2 block which state and directory need to be changed when refill */
				st->missq[qid].L2_changed_blk = blk;
			  }
			  break;

		  }
		  break;

		case SHARED:
		  switch(req){
			case READ_SHD:/* shared block receive shared read request, just return data */
			  if((!is_inst && blk->data_directory[request_cpuid]) ||
				  (is_inst && blk->inst_directory[request_cpuid])){
				/* dcache state is not coherent with scache directory, wait for directory state changing */
				st->missq[qid].wait_for_response = 1;
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
			  }else{
#ifdef MESI
				int dir_count = 0;
#endif
				/* local L1 cache miss ? */
				if (st->cpuid == request_cpuid){
				  missq_change_state(st,qid,MQ_REPLACE_L1);
				  st->missq[qid].memcnt = 4;
				}
				else{
				  missq_change_state(st,qid,MQ_EXTRDY);
				  st->missq[qid].memcnt = 4;
				}
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
				st->missq[qid].L2_changed_blk = NULL;
				st->missq[qid].wait_for_response = 0;
				if (is_inst)
				  blk->inst_directory[request_cpuid] = 1;
				else
				  blk->data_directory[request_cpuid] = 1;
#ifdef MESI
				if (!is_inst) {
				  for(i=0 ; i<total_cpus ; i++)
					dir_count += blk->data_directory[i];
				  blk->status = st->missq[qid].cache_status = (dir_count == 1 ? EXCLUSIVE : SHARED);
				}
#endif
			  }
			  break;

			case READ_EX:/* shared block receive exclusive read request, indicating a cpu doesn't hold a block and want hold a exclusive of this block, need to send invalidate request to other L1 */
			  assert(!is_inst);
			  if(blk->data_directory[request_cpuid]){
				/* dcache state is not coherent with scache directory, wait for directory state changing */
				st->missq[qid].wait_for_response = 1;
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
			  }else{
				missq_change_state(st,qid,MQ_MODIFY_L1);
				if (st->missq[qid].read_local_l2)
				  st->missq[qid].read_local_l2 = 0;
				for(i=0 ; i<total_cpus ; i++){
				  st->missq[qid].data_directory[i] = (i==request_cpuid) ? 0 : blk->data_directory[i];
				  st->missq[qid].inst_directory[i] = (i==request_cpuid) ? 0 : blk->inst_directory[i];
				  st->missq[qid].data_intervention_sent[i] = 0;
				  st->missq[qid].inst_intervention_sent[i] = 0;
				  st->missq[qid].data_ack_received[i] = 0;
				  st->missq[qid].inst_ack_received[i] = 0;
				}
				st->missq[qid].intervention_type = INVALIDATE;
				st->missq[qid].wait_for_response = 0;
				/* record L2 block which state and directory need to be changed when refill */
				st->missq[qid].L2_changed_blk = blk;
			  }
			  break;

			case UPGRADE:/* shared block receive upgrade request, indicating a cpu hold a shared block but want to hold exclusive of this block, need to send invalidate request to other L1 */
			  assert(!is_inst);
			  missq_change_state(st,qid,MQ_MODIFY_L1);
			  for(i=0 ; i<total_cpus ; i++){
				st->missq[qid].data_directory[i] = (i==request_cpuid) ? 0 : blk->data_directory[i];
				st->missq[qid].inst_directory[i] = (i==request_cpuid) ? 0 : blk->inst_directory[i];
				st->missq[qid].data_intervention_sent[i] = 0;
				st->missq[qid].inst_intervention_sent[i] = 0;
				st->missq[qid].data_ack_received[i] = 0;
				st->missq[qid].inst_ack_received[i] = 0;
			  }
			  st->missq[qid].intervention_type = INVALIDATE;
			  st->missq[qid].wait_for_response = 0;
			  /* record L2 block which state and directory need to be changed when refill */
			  st->missq[qid].L2_changed_blk = blk;
			  break;
		  }
		  break;
	  }

	  st->L2_hits++;
	  return;
	}
  }

  /* L2 miss, need to invalidate according L1 block when replace L2 block */

  missq_change_state(st,qid,MQ_L2_MISS);
  /* find replace block */
  int bindex = -1;/* bindex being -1 show that randomly replace */
  extern tick_t sim_cycle;
  if (bindex==-1) {
	bindex = (int)sim_cycle & (st->cache_dl2->assoc - 1);
	/* avoid replace last refilled way */
	if (set == st->cache_dl2->refill_set) {
	  bindex = st->cache_dl2->assoc - 1 - st->cache_dl2->refill_way;
	}
  }

  repl = CACHE_BINDEX(st->cache_dl2, st->cache_dl2->sets[set].blks, bindex);

  for(i=0 ; i<total_cpus ; i++){
	st->missq[qid].data_directory[i] = repl->data_directory[i];
	st->missq[qid].inst_directory[i] = repl->inst_directory[i];
	st->missq[qid].data_intervention_sent[i] = 0;
	st->missq[qid].inst_intervention_sent[i] = 0;
	st->missq[qid].data_ack_received[i] = 0;
	st->missq[qid].inst_ack_received[i] = 0;
  }
  /* need to modify local L1. */
  st->missq[qid].intervention_type = INTERVENTION_EXC;
  st->missq[qid].L2_replace_blk  = repl;
  st->missq[qid].L2_changed_blk = repl;
  st->missq[qid].L2_replace_paddr = (repl->tag << st->cache_dl2->tag_shift) + (repl->set << st->cache_dl2->set_shift);
  st->L2_misses++;
  return;
}


/* L2 miss and missq refill data from memory into L2 cache */
void scache_refill(tick_t now, int cpuid, int refill_L2i)
{
  struct godson2_cpu *st = &cpus[cpuid];
  struct cache_blk *repl;
  md_addr_t paddr,tag,set;
  int req, refill_way, i;

  repl = st->missq[refill_L2i].L2_replace_blk;
  paddr = st->missq[refill_L2i].paddr;
  tag = CACHE_TAG(st->cache_dl2, paddr);
  set = CACHE_SET(st->cache_dl2, paddr);
  refill_way = st->missq[refill_L2i].L2_replace_way;
  req = st->missq[refill_L2i].req;

  /* update block tags */
  repl->tag = tag;
  repl->set = set;
#ifdef MESI
  if (st->missq[refill_L2i].inst)
	repl->status = SHARED;
  else 
	st->missq[refill_L2i].cache_status = repl->status = EXCLUSIVE;
#else
  repl->status = (req == READ_SHD) ? SHARED : EXCLUSIVE;
#endif
  repl->dirty = 0;
  repl->sb_valid = repl->sb_dirty = 0;
  /* now update the block directory */
  for (i=0 ; i<total_cpus ; i++) {
    if(i==st->missq[refill_L2i].cpuid){
      if (st->missq[refill_L2i].inst){
		repl->inst_directory[i] = 1;
		repl->data_directory[i] = 0;
      }else{
		repl->data_directory[i] = 1;
		repl->inst_directory[i] = 0;
      }
    }else{
      repl->data_directory[i] = 0;
      repl->inst_directory[i] = 0;
    }
  }

  /* remember current refilling block */
  st->cache_dl2->refill_set = set;
  st->cache_dl2->refill_way = st->missq[refill_L2i].L2_replace_way;

  st->cache_dl2->refill_blk = repl;
  st->cache_dl2->refill_bitmap = 0;

  /* Blow away last tagset so we don't fake a hit */
  st->cache_dl2->last_tagset = 0xffffffff;

  /* change missq state */
  if(st->missq[refill_L2i].cpuid == st->cpuid)
    /* home cpu request L2 data */
    missq_change_state(st,refill_L2i,MQ_REPLACE_L1);
  else
    /* remote cpu request L2 data */
    missq_change_state(st,refill_L2i,MQ_EXTRDY);

}


/* wtbkq writeback local or remote request into L2 and directory */
void scache_writeback(tick_t now, int cpuid, int request_cpuid, int req, int /*md_addr_t*/ paddr,int is_inst)
{
  struct godson2_cpu * st = &cpus[cpuid];
  md_addr_t tag = CACHE_TAG(st->cache_dl2,paddr);
  md_addr_t set = CACHE_SET(st->cache_dl2,paddr);
  struct cache_blk *blk;

  assert(req==WRITEBACK || req==ELIMINATE);

  st->cache_dl2_wtbk_access++;

  for (blk = st->cache_dl2->sets[set].way_head; blk; blk = blk->way_next) {
	if (blk->tag == tag && blk->status != INVALID){
	  if (is_inst)
		blk->inst_directory[request_cpuid] = 0;/* no more contain this block */
	  else
		blk->data_directory[request_cpuid] = 0;/* no more contain this block */
	  if(req == WRITEBACK)   blk->dirty = 1;
	}
  }

}


/* find whether missq has an empty entry*/
int find_missq_empty_entry(struct godson2_cpu *st)
{
  int i, emptyi = -1;
  int nexti;
  i = nexti = (st->lasti[MQ_EMPTY] + 1) % missq_ifq_size;
  do{
	if (emptyi == -1 && st->missq[i].state == MQ_EMPTY){
	  emptyi = i;
	  break;
	} else
	  i = (i + 1) % missq_ifq_size;
  } while (i != nexti);

  return emptyi;
}

/* find whether missq has more than one empty entry*/
int find_missq_two_empty_entry(struct godson2_cpu *st)
{
  if (st->missq_num < missq_ifq_size - 1){
	int i, emptyi = -1;
	int nexti;
	i = nexti = (st->lasti[MQ_EMPTY] + 1) % missq_ifq_size;
	do{
	  if (emptyi == -1 && st->missq[i].state == MQ_EMPTY){
		emptyi = i;
		break;
	  } else
		i = (i + 1) % missq_ifq_size;
	} while (i != nexti);

	return emptyi;
  } else
	return -1;

}


/* find whether extinvnq has an empty entry*/
int find_extinvnq_empty_entry(struct godson2_cpu *st)
{
  int i, emptyi = -1;
  for(i=0 ; i<extinvnq_ifq_size ; i++){
    if(emptyi == -1 && st->extinvnq[i].state == MQ_EMPTY)
      emptyi = i;
  }

  return emptyi;
}

/* find whether wtbkq has an empty entry*/
int find_wtbkq_empty_entry(struct godson2_cpu *st)
{
  int i, emptyi = -1;
  for(i=0 ; i<wtbkq_ifq_size ; i++){
    if(emptyi == -1 && st->wtbkq[i].valid == 0)
      emptyi = i;
  }

  return emptyi;
}


/* each cpu cache to memory part */

void cache2mem_init(struct godson2_cpu *st)
{
  int i;
  st->missq = (struct miss_queue *)
    calloc(missq_ifq_size, sizeof(struct miss_queue));

  st->wtbkq = (struct writeback_queue *)
    calloc(wtbkq_ifq_size, sizeof(struct writeback_queue));

  st->extinvnq = (struct miss_queue *)
    calloc(extinvnq_ifq_size, sizeof(struct miss_queue));

  if (!st->missq || !st->wtbkq || !st->extinvnq) 
    fatal ("out of virtual memory");

  refill_init_free_list(st);

  st->missq_num = 0;
  st->wtbkq_num = 0;
  for (i = 0;i < MQ_STATE_NUM;i ++){
    st->lasti[i] = 0;
  }
  st->wtbkq_lasti = 0;
  memq_init_free_list(st);
  st->wtbkq_replacei = 0;
  st->memq_num = 0;
  st->memq_head = NULL;
}

void cache2mem(struct godson2_cpu *st)
{
  int i,j;
  int emptyi=-1,L1_missi=-1,read_L2i=-1,modify_L1i=-1,replace_L1i=-1,refill_L1i=-1,L2_missi=-1,memrefi=-1,refill_L2i=-1,extrdyi=-1;
  int missq_has_delay1 = 0;
  int wtbkq_dirtyi = -1;
  int wtbkq_has_empty = 0;
  int wtbkq_has_two_empty = 0;
  int new_upgrade_req = 0;
  int same_set = 0;
  int exit_missq = 0;
  int scache_port_occupied = 0;
  struct refill_packet *refill;

  int currenti[MQ_STATE_NUM];
  int statei;
  int nexti;

  for (statei = 0;statei < MQ_STATE_NUM;statei++){
    currenti[statei] = -1;
  }

  
  /*************************** miss queue **************************/
  
  for (statei = 0 ; statei < MQ_STATE_NUM ; statei++){
	nexti = i = (st->lasti[statei] + 1) % missq_ifq_size;
	do{
	  if (currenti[statei] == -1 && st->missq[i].state == statei){
		currenti[statei] = i;
		break;
	  }else
		i = (i + 1) % missq_ifq_size;
	} while (i != nexti);
  }

  emptyi = currenti[MQ_EMPTY];
  L1_missi = currenti[MQ_L1_MISS];
  read_L2i = currenti[MQ_READ_L2];
  modify_L1i = currenti[MQ_MODIFY_L1];
  replace_L1i = currenti[MQ_REPLACE_L1];
  extrdyi = currenti[MQ_EXTRDY];
  L2_missi = currenti[MQ_L2_MISS];
  memrefi = currenti[MQ_MEMREF];
  refill_L2i = currenti[MQ_REFILL_L2];
  refill_L1i = st->refilli;
  
  for (i = 0;i < missq_ifq_size;i ++){
    if (st->missq[i].state==MQ_DELAY1) {
      missq_has_delay1 = 1;
    }
  }

  /* receive L1 miss request and access L2, ensure the missq has at least two empty entry */

  if (emptyi != -1 && st->missq_num < missq_ifq_size-2) {
    /* lsq has a pending issued miss */
    if (st->lsq_missi != -1) {
      /* check for hit */
	  for (i=0;i<missq_ifq_size;i++) {
		if (st->cpuid == st->missq[i].cpuid && st->missq[i].state!=MQ_EMPTY && !st->missq[i].inst && BLOCK_MATCH(st->missq[i].paddr,st->lsq[st->lsq_missi].paddr)){
		  if(st->missq[i].state>MQ_L1_MISS && st->missq[i].req==READ_SHD && (st->lsq[st->lsq_missi].req==READ_EX || st->lsq[st->lsq_missi].req==UPGRADE)){
			/* read request have been issued, change write request into upgrade request */
			st->lsq[st->lsq_missi].req = UPGRADE;
			new_upgrade_req = 1;
		  }else{
			if(st->missq[i].state==MQ_L1_MISS && st->missq[i].req==READ_SHD && st->lsq[st->lsq_missi].req==READ_EX){
			  /* both read and write request haven't been issued, change original read request into write request */
			  st->missq[i].req = READ_EX;
			}else if( (st->missq[i].req==READ_EX || st->missq[i].req==UPGRADE) && (st->lsq[st->lsq_missi].req==READ_EX || st->lsq[st->lsq_missi].req == UPGRADE || st->lsq[st->lsq_missi].req == READ_SHD) ){
			  /* instinctively, a READ_SHD should not be issued after an UPGRADE,but when an external request invalidate local L1, then a READ_SHD can be issued after UPGRADE. */
			  new_upgrade_req = 0;
			}
			/* miss request has been in missq, do not allow this new miss to enter, so break here. */
			break;
		  }
		}
	  }

	  if (i==missq_ifq_size || new_upgrade_req) {
		if(VALID_PADDR(st->lsq[st->lsq_missi].paddr)){
		  /* miss from data cache */
		  missq_change_state(st,emptyi,MQ_L1_MISS);
		  st->missq[emptyi].inst = 0;
		  st->missq[emptyi].set  = st->lsq[st->lsq_missi].set;
		  st->missq[emptyi].req  = st->lsq[st->lsq_missi].req;
		  st->missq[emptyi].ack  = NO_REQ;
		  st->missq[emptyi].cpuid = st->cpuid;/* self cpu L1 miss */
		  st->missq[emptyi].paddr = st->lsq[st->lsq_missi].paddr;
		  st->missq[emptyi].memcnt = 0;
		  st->missq[emptyi].cachecnt = 0;
		  st->missq[emptyi].wait_for_reissue = 0;
		  st->missq[emptyi].wait_for_response = 0;
		  st->missq[emptyi].memread_sent = 0;
		  st->missq[emptyi].lsconten = 0;
		  st->missq_num++;
		  {
			int pos = 0;
			int dest_x = PADDR_OWNER(st->lsq[st->lsq_missi].paddr) % mesh_width;
			int dest_y = PADDR_OWNER(st->lsq[st->lsq_missi].paddr) / mesh_width;
			int home_x = st->cpuid % mesh_width;
			int home_y = st->cpuid / mesh_width;

			if (home_x == dest_x && home_y == dest_y)
			  pos = 0;
			else
			  pos = 1 + (home_x/2 != dest_x/2) + (home_y/2 != dest_y/2);
			stat_add_sample(st->l1dmiss_dist, pos);
		  }
		}else{
		  /* invalid paddr, don't touch the request */
		}
	  }
      /* tell lsq the request has been accepted */
      st->lsq[st->lsq_missi].fetching = 1;
      st->lsq_missi = -1;
      st->missq_access++;
      st->dcache_read_low_level_count ++;
    } else if (st->irepbuf.state==IREP_MISS) {
	  if(VALID_PADDR(st->irepbuf.paddr)){
		/* miss from instruction cache */
		/* update credit in missq_change_state.*/
		missq_change_state(st,emptyi,MQ_L1_MISS);
		st->missq[emptyi].inst = 1;
		st->missq[emptyi].set = st->irepbuf.set;
		st->missq[emptyi].req = READ_SHD;
		st->missq[emptyi].ack  = NO_REQ;
		st->missq[emptyi].cpuid = st->cpuid;
		st->missq[emptyi].paddr = st->irepbuf.paddr;
		st->missq[emptyi].memcnt = 0;
		st->missq[emptyi].cachecnt = 0;
		st->missq[emptyi].wait_for_reissue = 0;
		st->missq[emptyi].wait_for_response = 0;
		st->missq[emptyi].memread_sent = 0;
		st->missq_num++;
	  }else{
	    /* invalid paddr, don't touch the request */
	  }
      /* tell irepbuf the request has been accepted */
      st->irepbuf.state = IREP_REFILL;
      st->missq_access++;
      st->icache_read_low_level_count ++;
    }
  }


  /* try to issue memory read request */

  if (L1_missi != -1 && !st->missq[L1_missi].wait_for_reissue) {
    md_addr_t paddr = st->missq[L1_missi].paddr;
    int dest_cpuid = PADDR_OWNER(paddr);

    /* if this miss is initiated by remote cpu, the physical address should be in home address space */
    if (st->cpuid != st->missq[L1_missi].cpuid)
      assert(dest_cpuid == st->cpuid);
    /* ensure this miss is of not the same set with issued request and UPGRADE will not be issued before a former READ_SHD receives its ack.*/
    for(i=0;i<missq_ifq_size;i++){
	  if(i!=L1_missi &&
		 (st->missq[i].state>MQ_L1_MISS && CACHE_SET(st->cache_dl2,st->missq[i].paddr)==CACHE_SET(st->cache_dl2,st->missq[L1_missi].paddr) && PADDR_OWNER(st->missq[i].paddr)==PADDR_OWNER(st->missq[L1_missi].paddr)))
		same_set = 1;
	  if(i!=L1_missi && st->cpuid == st->missq[i].cpuid && st->cpuid == st->missq[L1_missi].cpuid &&
		 st->missq[i].state >= MQ_L1_MISS && BLOCK_MATCH(st->missq[i].paddr,st->missq[L1_missi].paddr) &&
		 st->missq[i].req == READ_SHD && st->missq[L1_missi].req == UPGRADE) {
		/* load-store contention */
		if (!st->missq[L1_missi].lsconten) {
		  st->sim_load_store_contention++;
		  st->missq[L1_missi].lsconten = 1;
		}
		same_set = 1;
		st->lasti[MQ_L1_MISS] = (st->lasti[MQ_L1_MISS] + 1) % missq_ifq_size;
	  }
    }
	
    if (!same_set){
	  if (dest_cpuid == st->cpuid) {

		/* local L2 cache access */
		missq_change_state(st,L1_missi,MQ_READ_L2);
		st->missq[L1_missi].read_local_l2 = 1;
		eventq_queue_callback4(sim_cycle + L2_probe_delay * st->period, scache_probe, st->cpuid, st->missq[L1_missi].req, L1_missi, paddr);
		scache_port_occupied = 1;
		st->cache_dl2_fetch_access++;
	  } else if (noc_request(st, REQ, st->missq[L1_missi].req,0, L1_missi, st->cpuid, paddr,st->missq[L1_missi].inst)) {
		/* remote L2 cache access */
		missq_change_state(st,L1_missi,MQ_READ_L2);
		st->missq[L1_missi].read_local_l2 = 0;
		st->noc_read_count++;
	  }
	}
  }


  /* router return data, so change missq state to replace_L1 */

  if(read_L2i != -1){
    if(st->missq[read_L2i].wait_for_response == 2){
      /* L1 and L2 are not coherent, and now response arrive directory, so reprobe scache */
      scache_probe(sim_cycle, st->cpuid, st->missq[read_L2i].req, read_L2i, st->missq[read_L2i].paddr);
    }
  }

  
  /* miss in L1 and hit in L2, but need to send intervention request to modify other L1 state */
  
  if (modify_L1i != -1) {
    /* check for internal conflict request.*/	
    /* external intervention request */
    if (st->missq[modify_L1i].req == INTERVENTION_SHD || st->missq[modify_L1i].req == INTERVENTION_EXC || st->missq[modify_L1i].req == INVALIDATE) {
	  if (!st->missq[modify_L1i].conflict_checked){
		st->missq[modify_L1i].invn_match_i1 = st->missq[modify_L1i].invn_match_i2 = -1;
		/* check missq, find the block-match paddr */
		for(i=0 ; i<missq_ifq_size ; i++){
		  if(i != modify_L1i && st->missq[i].state!=MQ_EMPTY && BLOCK_MATCH(st->missq[i].paddr,st->missq[modify_L1i].paddr)){
			if(st->missq[i].state == MQ_L1_MISS){
			  /* internal conflict, the missq request has not been issued, the missq should wait for the completion of external intervention, and then reprobe the dcache and make lsq reissue this request */
			  st->missq[i].wait_for_reissue = 1;
			  /* record missq entry which block match with external intervention paddr, at most two entries */
			  if(st->missq[modify_L1i].invn_match_i1 == -1)
				st->missq[modify_L1i].invn_match_i1 = i;
			  else
				st->missq[modify_L1i].invn_match_i2 = i;

			}else{
			  /* external conflict, the missq request has been issued to directory, the missq should wait for ack, so nothing to do here */
			}
		  }
		}
		st->missq[modify_L1i].conflict_checked = 1;
	  }
	  
	  if (!st->missq[modify_L1i].wtbkq_checked){
		/* check wtbkq for conflict write back */
		for(i=0 ; i<wtbkq_ifq_size ; i++){
		  if(st->wtbkq[i].valid && BLOCK_MATCH(st->wtbkq[i].paddr,st->missq[modify_L1i].paddr) && st->wtbkq[i].inst == st->missq[modify_L1i].inst){
			/* internal conflict, the write back request has not been issued, missq should change the write back into a response for this external coherency request. */
			st->missq[modify_L1i].ack = (st->wtbkq[i].req == WRITEBACK) ? ACK_DATA :ACK;
			st->missq[modify_L1i].cache_status = st->wtbkq[i].cache_status;
			if (!(st->missq[modify_L1i].req == INTERVENTION_SHD && 
				  (st->wtbkq[i].cache_status == EXCLUSIVE || st->wtbkq[i].cache_status == MODIFIED))) {
			  st->wtbkq[i].valid = 0;
			  eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, WTBK, HOME);
			  st->wtbkq_num--;
			  assert(st->wtbkq_num >=0);
			}
			missq_change_state(st,modify_L1i,MQ_EXTRDY);
			exit_missq = 1;
			break;
		  }
		}
		st->missq[modify_L1i].wtbkq_checked = 1;
	  }
	}else{
      /* local L1 cache has been modified through refill bus, or local L1 cache need not to be modified. modify remote L1 caches. */
	  for(i=0 ; i<total_cpus ; i++){
		/* not local L1 cache and request not sent. */
		if(i != st->cpuid && st->missq[modify_L1i].data_directory[i] == 1 && 
			st->missq[modify_L1i].data_intervention_sent[i] != 1){
		  if(noc_request(st, INVN, st->missq[modify_L1i].intervention_type,0, 
				modify_L1i, i, st->missq[modify_L1i].paddr,0)){
			st->missq[modify_L1i].data_intervention_sent[i] = 1;
			break;
		  }
		}
		if (st->missq[modify_L1i].L2_changed_blk->status == SHARED){
		  if(i != st->cpuid && st->missq[modify_L1i].inst_directory[i] == 1 && 
			  st->missq[modify_L1i].inst_intervention_sent[i] != 1){
			if(noc_request(st, INVN, st->missq[modify_L1i].intervention_type,0, 
				  modify_L1i, i, st->missq[modify_L1i].paddr,1)){
			  st->missq[modify_L1i].inst_intervention_sent[i] = 1;
			  break;
			}
		  }
		}
	  }
	  
	  /* if all ack received, change missq entry state to REPLACE_L1 */
      int ack_all_received = 1;
	  for(i=0 ; i<total_cpus ; i++){
		if((st->missq[modify_L1i].data_directory[i] == 1 && st->missq[modify_L1i].data_ack_received[i] != 1) ||
			(st->missq[modify_L1i].data_intervention_sent[i] != st->missq[modify_L1i].data_ack_received[i]))
		  ack_all_received = 0;
		if((st->missq[modify_L1i].inst_directory[i] == 1 && st->missq[modify_L1i].inst_ack_received[i] != 1) ||
			(st->missq[modify_L1i].inst_intervention_sent[i] != st->missq[modify_L1i].inst_ack_received[i]))
		  ack_all_received = 0;
	  }
	  
	  if(ack_all_received){
#ifdef MESI
		int dir_count = 0;
#endif
		/* modify L2 cache directory and status here.*/
		struct cache_blk *blk = st->missq[modify_L1i].L2_changed_blk;
		int req = st->missq[modify_L1i].req;
		int cpuid = st->missq[modify_L1i].cpuid;
		int is_inst = st->missq[modify_L1i].inst;
		/* READ_SHD hit a SHARED L2 blk, do nothing ... */
		if(blk){
		  /* if READ_SHD hit a shared L2 blk, here blk will be null. */
		  /* copy directory from missq to L2. */
		  for (i = 0;i <total_cpus;i++){
			blk->data_directory[i] = st->missq[modify_L1i].data_directory[i];
#ifdef MESI
			dir_count += blk->data_directory[i];
#endif
			blk->inst_directory[i] = st->missq[modify_L1i].inst_directory[i];
		  }
		  if(req==READ_SHD){
#ifdef MESI
			if (is_inst) {
			  blk->status = SHARED;
			} else {
			  /* st->missq[modify_L1i].cache_status = blk->status = dir_count == 0 ? EXCLUSIVE : SHARED; */
			  blk->status = st->missq[modify_L1i].cache_status;
			}
#else
			blk->status = SHARED;
#endif
			if (is_inst)
			  blk->inst_directory[cpuid] = 1;
			else
			  blk->data_directory[cpuid] = 1;
		  }else{
			blk->status = EXCLUSIVE;
			for(i=0;i<total_cpus;i++){
			  blk->data_directory[i] = (i==cpuid) ? 1 : 0;
			  blk->inst_directory[i] = 0;
			}
		  }
		}
		/* local L1 cache miss hit in local L2, change state to REPLACE_L1. */
		if (st->cpuid == st->missq[modify_L1i].cpuid){
		  missq_change_state(st,modify_L1i,MQ_REPLACE_L1);
		  st->missq[modify_L1i].memcnt = 4;/* set memcnt to 4 to indicate the data has returned */
		}else{// remote L1 miss hit in L2, change state to EXTRDY.
		  missq_change_state(st,modify_L1i,MQ_EXTRDY);
		  st->missq[modify_L1i].memcnt = 4;
		}
	  }
	}
	st->lasti[MQ_MODIFY_L1] = modify_L1i;
  }


  /* L2 miss, send request to memory controller, and at the same time invalidate all L1 blocks which contains according L2 block that is to replace */

  if (L2_missi != -1) {
    /* send invalidate and writeback request */
    if (st->missq[L2_missi].L2_replace_blk->status != INVALID){
	  for(i=0 ; i<total_cpus ; i++){
		/* if this INTERVENTION_EXC need not to send to local L1, send it to router-> */
		if(st->missq[L2_missi].data_directory[i] == 1 && st->missq[L2_missi].data_intervention_sent[i] != 1 && i != st->cpuid){
		  if(noc_request(st,INVN,INTERVENTION_EXC,0,L2_missi,i,st->missq[L2_missi].L2_replace_paddr,0)){
			st->missq[L2_missi].data_intervention_sent[i] = 1;
			break;
		  }
		}
		
		if (st->missq[L2_missi].L2_changed_blk->status == SHARED){
		  if(st->missq[L2_missi].inst_directory[i] == 1 && st->missq[L2_missi].inst_intervention_sent[i] != 1 && i != st->cpuid){
			if(noc_request(st, INVN,INTERVENTION_EXC ,0, L2_missi, i, st->missq[L2_missi].L2_replace_paddr,1)){
			  st->missq[L2_missi].inst_intervention_sent[i] = 1;
			  break;
			}
		  }
		}
	  }
	}

	int ack_all_received = 1;
	for(i=0 ; i<total_cpus ; i++){
	  if((st->missq[L2_missi].data_directory[i] == 1 && st->missq[L2_missi].data_ack_received[i] != 1) ||
		  (st->missq[L2_missi].data_intervention_sent[i] != st->missq[L2_missi].data_ack_received[i]))
		ack_all_received = 0;
	  if ((st->missq[L2_missi].inst_directory[i] == 1 && st->missq[L2_missi].inst_ack_received[i] != 1) ||
		  (st->missq[L2_missi].inst_intervention_sent[i] != st->missq[L2_missi].inst_ack_received[i]))
		ack_all_received = 0;
	}
	/* send request to memory controller */
	if (!st->missq[L2_missi].memread_sent)
	  st->missq[L2_missi].memread_sent = memq_enter(st, Read,L2_missi);
	if (ack_all_received && st->missq[L2_missi].memread_sent){
	  missq_change_state(st,L2_missi,MQ_MEMREF);
	  st->memq_read_count++;
	}
  }


  /* now we are doing access to memory, and go on sending invalidate request if there have remaining haven't_sent request */

  if (memrefi != -1) {
    /* send invalidate request */
#if 0 // this work should be done in MQ_L2_MISS
    for(i=0 ; i<total_cpus ; i++){
      if(st->missq[memrefi].directory[i] == 1 && st->missq[memrefi].intervention_sent[i] != 1){
	if(noc_request(st, INVN, INVALIDATE, memrefi, i, st->missq[memrefi].L2_replace_paddr))    st->missq[memrefi].intervention_sent[i] = 1;
      }
    }
    /* if all ack received and memory controller have return data, change missq entry state to REFILL_L2 */
    int ack_all_received = 1;
    for(i=0 ; i<total_cpus ; i++){
      if(st->missq[memrefi].directory[i] == 1 && st->missq[memrefi].ack_received[i] != 1)    ack_all_received = 0;
    }
#endif

    if(st->missq[memrefi].memcnt == 4){
      missq_change_state(st,memrefi,MQ_REFILL_L2);
      st->missq[memrefi].scache_refilled = 0;
      st->missq[memrefi].memwrite_sent = 0;
    }

  }


  /* L2 block has been replaced and memory controller has return data, now refill the data to L2 */

  if (refill_L2i != -1) {
    if (st->missq[refill_L2i].memwrite_sent || !st->missq[refill_L2i].L2_replace_blk->dirty){
      if (!st->missq[refill_L2i].scache_refilled){
		eventq_queue_callback2(sim_cycle + L2_write_delay * st->period, scache_refill, st->cpuid, refill_L2i);
		st->missq[refill_L2i].scache_refilled = 1;
		st->cache_dl2_refill_access++;
      }
    }else{
      st->missq[refill_L2i].memwrite_sent = memq_enter(st,Write,refill_L2i);
    }
#if 0
    /* change missq state */
    if(st->missq[refill_L2i].cpuid == st->cpuid)
      /* home cpu request L2 data */
      st->missq[refill_L2i].state = MQ_REPLACE_L1;
    else
      /* remote cpu request L2 data */
      st->missq[refill_L2i].state = MQ_EXTRDY;
#endif
  }


  /************************* writeback queue ***********************/ 

  for (i=st->wtbkq_lasti,j=0 ; j<wtbkq_ifq_size ; i=(i+1)%wtbkq_ifq_size,j++) {
    if (wtbkq_dirtyi==-1 && st->wtbkq[j].valid) {
      wtbkq_dirtyi=j;
    }
#if 0 
    //now we use wtbkq_num to determine wtbkq_has_empty.
    else if (!st->wtbkq[i].valid) {
      /* writeback queue has an empty entry */
      wtbkq_has_empty = 1;
    }
#endif 
  }

  /* each cycle update wtbkq_lasti once */
  st->wtbkq_lasti = (st->wtbkq_lasti + 1) % wtbkq_ifq_size;

  wtbkq_has_empty = st->wtbkq_num < wtbkq_ifq_size;
  wtbkq_has_two_empty = st->wtbkq_num < wtbkq_ifq_size - 1;


  /* try to issue memory write request, note that read request is processed first */

  if (wtbkq_dirtyi!=-1) {
    md_addr_t paddr = st->wtbkq[wtbkq_dirtyi].paddr;
    int dest_cpuid = PADDR_OWNER(paddr);

    /* if this write back is destined for this cpu, check missq for the actual directory location. */
    if (dest_cpuid == st->cpuid) {
      /* check missq for conflict L1 miss request. */
      int should_issue_wtbk = 1;

	  for (i = 0; i < missq_ifq_size; i++){
		if ((st->missq[i].state == MQ_MODIFY_L1 && BLOCK_MATCH(st->missq[i].paddr,(md_addr_t)paddr)) ||
			(st->missq[i].state == MQ_L2_MISS && BLOCK_MATCH(st->missq[i].L2_replace_paddr,(md_addr_t)paddr)) ||
			(st->missq[i].state == MQ_READ_L2 && BLOCK_MATCH(st->missq[i].paddr,(md_addr_t)paddr)))
		{
		  /* if an external read request is waiting in missq for a write back because 
		   * its read request suggest an non-coherent cache state with L2, let it 
		   * continue and write back this L1 into L2. This may be not identical to 
		   * hardware implementation since L2 directory should have been read into 
		   * missq when the external read request arrived. we write this way for 
		   * convinient.(not need to write what have done in scache_probe() a second 
		   * time in read_L2i. */
		  if (st->missq[i].state == MQ_READ_L2){
			if (st->missq[i].read_local_l2){
			  /* wait for read complete! */
			}else if (st->missq[i].wait_for_response == 1 || st->missq[i].wait_for_response == 2 ) {
			  if (st->missq[i].cpuid == st->wtbkq[wtbkq_dirtyi].cpuid)
				st->missq[i].wait_for_response = 2;/* do not set should_issue_wtbk to zero here! */
			  scache_writeback(sim_cycle,st->cpuid,st->wtbkq[wtbkq_dirtyi].cpuid,st->wtbkq[wtbkq_dirtyi].req, st->wtbkq[wtbkq_dirtyi].paddr,st->wtbkq[wtbkq_dirtyi].inst);
			  st->wtbkq[wtbkq_dirtyi].valid = 0;
			  eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, WTBK, HOME);
			  st->wtbkq_num--;
			  assert(st->wtbkq_num >=0);
			}else {
			  assert(0);
			}
		  } else if (st->missq[i].state == MQ_MODIFY_L1 || st->missq[i].state == MQ_L2_MISS){
			/* doing L1 modification. */
			if (st->wtbkq[wtbkq_dirtyi].intervention){ 
			  /* local cpu send intervention to local l1. need to get ack here
			   * now only instruction here. */
			  assert(st->wtbkq[wtbkq_dirtyi].inst);
			  if (st->wtbkq[wtbkq_dirtyi].cache_status == SHARED)
				st->missq[i].inst_ack_received[st->wtbkq[wtbkq_dirtyi].cpuid] = 1;
			  else{
				assert(st->wtbkq[wtbkq_dirtyi].cache_status == INVALID);
				st->missq[i].inst_intervention_sent[st->wtbkq[wtbkq_dirtyi].cpuid] = 0;
			  }
			} else {
			  if (st->wtbkq[wtbkq_dirtyi].inst){
				/* since invalidate request doesn't receive real ack, the ack is 
				 * received by wtbkq, so we update directory in missq here to 
				 * simulate the ack. */
				st->missq[i].inst_directory[st->wtbkq[wtbkq_dirtyi].cpuid] = 0;
			  } else
				st->missq[i].data_directory[st->wtbkq[wtbkq_dirtyi].cpuid] = 0;
			}
			st->wtbkq[wtbkq_dirtyi].valid = 0;
			eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, 
				st->router->home_y, WTBK, HOME);
			st->wtbkq_num--;
			assert(st->wtbkq_num >=0);
		  } else{
			assert(0);
		  }
		  should_issue_wtbk = 0;
		}
	  }
	  /* L1 write back and L2 cache probe should be serialized to ensure that missq will not read an old state of directory. We check two conditions here to guarantee that when a L1 write back to L2, no pending L1 probe has not been received by missq:
	   * 1. no missq entry is in READ_L2 state and is reading local L2 cache and it is not waiting for remote writeback, and it is L2 cache index(since L2 read may miss and read out a block which is going to be replaced) matched with this writeback. (these latter two conditions must be added since replace may be blocked by a full wtbkq, and writeback may be blocked by a read_L2i in missq, while the read_L2i is blocked by a non-coherent block read which can only be ended by a new writeback which can not enter the wtkbq... deadlock will occur!!!)
	   * 2. no missq entry is issuing a scache probe.(scache_port_occupied == 0).*/	
	  if (should_issue_wtbk){
		for(i=0 ; i<missq_ifq_size ; i++){
		  if ((st->missq[i].state == MQ_READ_L2 && st->missq[i].read_local_l2 && 
/*				!st->missq[i].wait_for_response && */
				/* it seems obove condition is not necessary. */
				(CACHE_SET(st->cache_dl2,st->missq[i].paddr) == CACHE_SET(st->cache_dl2,paddr))) || 
			  scache_port_occupied){
			should_issue_wtbk = 0;
			break;
		  }
		}
		if (should_issue_wtbk){
		  //md_addr_t paddr = st->wtbkq[wtbkq_dirtyi].paddr;
		  //int dest_cpuid = PADDR_OWNER(paddr);

		  /* local L2 cache access */
		  st->wtbkq[wtbkq_dirtyi].valid = 0;
		  eventq_queue_callback5(sim_cycle + L2_write_delay * st->period, scache_writeback, st->cpuid, st->wtbkq[wtbkq_dirtyi].cpuid, st->wtbkq[wtbkq_dirtyi].req, st->wtbkq[wtbkq_dirtyi].paddr,st->wtbkq[wtbkq_dirtyi].inst);
		  /*update home wtbkq credit*/
		  eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, WTBK, HOME);
		  st->wtbkq_num--;
		  assert(st->wtbkq_num >=0);
		  scache_port_occupied = 1;
		}
	  }

	} else {
	  /* check external coherency request here. though we have checked it when the external request entered the missq, external coherency request (current only for instruction in this simulator) must receive their ack through replace_block() -> wtbkq ->missq. obviously we only check external coherency request. */
	  /* this comment has outdated: because this check is not necessary for the correct implementation of cache coherency protocol, but for the consideration of efficiency.*/
	  if (st->wtbkq[wtbkq_dirtyi].intervention){/* this write back is invoked by an intervention */
		int found = 0;
		for (i = 0; i < missq_ifq_size; i++){
		  if (st->missq[i].state == MQ_MODIFY_L1 && BLOCK_MATCH(st->missq[i].paddr,(md_addr_t)paddr)){
			st->missq[i].ack = (st->wtbkq[wtbkq_dirtyi].cache_status == MODIFIED) ? ACK_DATA : ACK;
			st->missq[i].cache_status = st->wtbkq[wtbkq_dirtyi].cache_status;
			missq_change_state(st,i,MQ_EXTRDY);
			found = 1;
		  }
		}
		assert(found);
		st->wtbkq[wtbkq_dirtyi].valid = 0;
		/*update home wtbkq credit*/
		eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, WTBK, HOME);
		st->wtbkq_num--;
		assert(st->wtbkq_num >=0);
	  } else if (noc_request(st, WTBK, st->wtbkq[wtbkq_dirtyi].req,0, wtbkq_dirtyi, st->cpuid, paddr,st->wtbkq[wtbkq_dirtyi].inst)) {
		/* remote L2 cache access */
		st->wtbkq[wtbkq_dirtyi].valid = 0;
		/*update home wtbkq credit*/
		eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, WTBK, HOME);
		st->wtbkq_num--;
		assert(st->wtbkq_num >=0);
		st->noc_write_count++;
	  }
	}
  }


  /* external request ready */

  if (extrdyi != -1){
	/* external intervention request return ack */
	if (st->missq[extrdyi].req == INTERVENTION_SHD || st->missq[extrdyi].req == INTERVENTION_EXC ||
		st->missq[extrdyi].req == INVALIDATE) {
	  assert(st->missq[extrdyi].ack != NO_REQ);
	  if(noc_request(st, RESP, st->missq[extrdyi].ack ,st->missq[extrdyi].cache_status, st->missq[extrdyi].qid, st->missq[extrdyi].cpuid, st->missq[extrdyi].paddr,st->missq[extrdyi].inst)){
		missq_change_state(st,extrdyi,MQ_EMPTY);
		st->missq_num--;
		/* update home extinvnq credit */
		eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, INVN, HOME);
	  }
	} else {
	  /* refill data into remote request cpu missq */
	  if(noc_request(st, RESP, ACK_DATA,st->missq[extrdyi].cache_status, st->missq[extrdyi].qid, st->missq[extrdyi].cpuid, st->missq[extrdyi].paddr,st->missq[extrdyi].inst)){
#if 0
		/* change L2 status earlier in MODIFY_L1 after all ack received . */
		/* change L2 block state and directory */
		if(st->missq[extrdyi].memcnt == 4){
		  struct cache_blk *blk = st->missq[extrdyi].L2_changed_blk;
		  int req = st->missq[extrdyi].req;
		  int cpuid = st->missq[extrdyi].cpuid;
		  int is_inst = st->missq[extrdyi].inst;
		  if(blk){
			/* if READ_SHD hit a shared L2 blk, here blk will be null. */
			if(req==READ_SHD){
			  blk->status = SHARED;
			  if (is_inst)
				blk->inst_directory[cpuid] = 1;
			  else
				blk->data_directory[cpuid] = 1;
			}else{
			  blk->status = EXCLUSIVE;
			  for(i=0;i<total_cpus;i++){
				blk->data_directory[i] = (i==cpuid) ? 1 : 0;
				blk->inst_directory[i] = 0;
			  }
			}
		  }
		}
#endif
		missq_change_state(st,extrdyi,MQ_EMPTY);
		st->missq_num--;
		/* update home missq credit */
		eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, REQ, HOME);
	  }
	}
  }

  
  /***************** replace and refill L1 part *******************/

  if (st->refill_delay > 0){
    st->refill_delay --;
  }

  
  /* doing refill, no other refills and interventions can be inserted. */
  
  if (st->refilltag) {
	
	/* refill data into local L1 */
	if (st->missq[refill_L1i].cachecnt < st->missq[refill_L1i].memcnt) {
	  int b; /* block index */
	  b = st->missq[refill_L1i].cachecnt ^ ((st->missq[refill_L1i].paddr & 0x1f) >> 3);
	  st->missq[refill_L1i].cachecnt++;

	  if (st->missq[refill_L1i].inst) {
		/* icache case is simpler,no need to give it a refill packet */
		st->icache_refill_access++;
		st->fetch_istall_buf.stall |= REFILL_STALL;
		st->irepbuf.bitmap[b] = 1;
		if (st->missq[refill_L1i].cachecnt==4) {
		  mark_valid(st->icache,SHARED);
		  st->icache->refill_blk = NULL;
		  st->irepbuf.state = IREP_EMPTY;
		}
	  } else {
		refill = refill_get_from_free_list(st);
		refill->replace = 0;
		refill->intervention = 0;
		refill->set = st->missq[refill_L1i].set;
		refill->paddr = (st->missq[refill_L1i].paddr & (~0x1f)) + (b << 3);
		refill->cnt = st->missq[refill_L1i].cachecnt;
		refill->req = st->missq[refill_L1i].req;
#ifdef MESI
		refill->cache_status = st->missq[refill_L1i].cache_status;
#endif
		/* pass refill packet to L1 */
		st->lsq_refill = refill;
		st->dcache_refill_access++;
	  }

	  if (st->missq[refill_L1i].cachecnt==4) {
		st->refilltag = 0;
		st->refill_delay = 4;
		missq_change_state(st,refill_L1i,MQ_DELAY1);
	  }

	}
	
  } else if ( modify_L1i != -1  && wtbkq_has_two_empty && !missq_has_delay1 && 
	  ((st->missq[modify_L1i].req == INTERVENTION_SHD || st->missq[modify_L1i].req == INTERVENTION_EXC || st->missq[modify_L1i].req == INVALIDATE) && st->missq[modify_L1i].modify_l1)) {
	/* should do st->wtbkq_num--; but we fake the replace. */
	  if (!st->refill_delay){
		if (st->missq[modify_L1i].wtbkq_checked && !exit_missq){
		  if (!st->missq[modify_L1i].inst){
			/* clear the conflict internal request. three cycles later lsq will receive this intervention and meanwhile the conflict internal request will be cleared. */

			int invn_matchi = st->missq[modify_L1i].invn_match_i1;
			if(invn_matchi != -1 && st->missq[invn_matchi].state == MQ_L1_MISS &&
			   BLOCK_MATCH(st->missq[modify_L1i].paddr, st->missq[invn_matchi].paddr)){
			  st->missq[invn_matchi].state = MQ_DELAY1;
			}
			invn_matchi = st->missq[modify_L1i].invn_match_i2;
			if(invn_matchi != -1  && st->missq[invn_matchi].state == MQ_L1_MISS &&
               BLOCK_MATCH(st->missq[modify_L1i].paddr, st->missq[invn_matchi].paddr)){
			  st->missq[invn_matchi].state = MQ_DELAY1;
			}

			/* notify the dcache there is an intervention op */

			refill = refill_get_from_free_list(st);
			refill->replace = 0;
			/* is intervention op */
			refill->intervention = 1;
			refill->req = st->missq[modify_L1i].req;
			refill->paddr = st->missq[modify_L1i].paddr;
			refill->missqid = modify_L1i;
			st->lsq_refill = refill;
			st->missq[modify_L1i].modify_l1 = 0;
			st->dcache_refill_access++;
			//st->missq[modify_L1i].data_intervention_sent[st->cpuid] = 1;
			/* change missq state later when ack received.*/
			//st->missq[modify_L1i].state = MQ_REFILL_L1;
		  }else{
			int invn_matchi = st->missq[modify_L1i].invn_match_i1;
			
			st->wtbkq_num++;
			assert(st->wtbkq_num <= wtbkq_ifq_size);
			st->router->egress_credit[WTBK][HOME]--;
			assert(st->router->egress_credit[WTBK][HOME] >= 0);
			/* need to modify instruction cache... */
			replace_block(st->icache,st->irepbuf.way,st->missq[modify_L1i].paddr,NO_REQ,1,1, UNTOUCHED);
			st->replace_access++;
			st->fetch_istall_buf.stall |= REFILL_STALL;
			//st->missq[modify_L1i].inst_intervention_sent[st->cpuid] = 1;
			st->missq[modify_L1i].modify_l1 = 0;
			
			if(invn_matchi != -1){
			  st->missq[invn_matchi].state = MQ_EMPTY;
			  st->missq_num--;
			  /* update home extinvnq credit */
			  eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, INVN, HOME);
			}
			invn_matchi = st->missq[modify_L1i].invn_match_i2;
			if(invn_matchi != -1){
			  st->missq[invn_matchi].state = MQ_EMPTY;
			  st->missq_num--;
			  /* update home extinvnq credit */
			  eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, INVN, HOME);
			}
		  }
		}
	  }
	} else if ( modify_L1i != -1  && wtbkq_has_two_empty && !missq_has_delay1 &&
		((st->missq[modify_L1i].data_directory[st->cpuid] && !st->missq[modify_L1i].data_intervention_sent[st->cpuid]) || 
		  (st->missq[modify_L1i].inst_directory[st->cpuid] && !st->missq[modify_L1i].inst_intervention_sent[st->cpuid]))) {
	  /* L2 cache hit, need to modify L1 cache state. */
	  if (st->missq[modify_L1i].L2_changed_blk->status != INVALID){
		if (st->missq[modify_L1i].inst_directory[st->cpuid] == 1 && st->missq[modify_L1i].inst_intervention_sent[st->cpuid] != 1){
		  st->wtbkq_num++;
		  assert(st->wtbkq_num <= wtbkq_ifq_size);
		  st->router->egress_credit[WTBK][HOME]--;
		  assert(st->router->egress_credit[WTBK][HOME] >= 0);

		  replace_block(st->icache,st->irepbuf.way,st->missq[modify_L1i].paddr,NO_REQ,1,1, UNTOUCHED);
		  st->replace_access++;
		  st->missq[modify_L1i].inst_intervention_sent[st->cpuid] = 1;
		  st->fetch_istall_buf.stall |= REFILL_STALL;
		} else if (st->missq[modify_L1i].data_directory[st->cpuid] && !st->missq[modify_L1i].data_intervention_sent[st->cpuid] && !st->refill_delay){
		  /* remote L1 miss hit in local L2, and L2 cache ready, need to modify local L1 cache state through refill bus.*/
		  refill = refill_get_from_free_list(st);
		  refill->replace = 0;
		  /* is intervention op */
		  refill->intervention = 1;
		  refill->req = st->missq[modify_L1i].intervention_type;
		  refill->paddr = st->missq[modify_L1i].paddr;
		  refill->missqid = modify_L1i;
		  st->lsq_refill = refill;
		  st->dcache_refill_access++;
		  st->missq[modify_L1i].data_intervention_sent[st->cpuid] = 1;
		}
	  }
	
  } else if (replace_L1i != -1 && /*wtbkq_has_empty*/wtbkq_has_two_empty && !missq_has_delay1) {

	/* record that we are doing replace and refill, no other operations except read L1 cache and dcachewrite are allowed. */
	st->refilltag = 1;
	st->refilli = replace_L1i;
	st->replace_access++;
	/* do this in advance to avoid write back delay. */
	st->wtbkq_num++;
	assert(st->wtbkq_num <= wtbkq_ifq_size);
	st->router->egress_credit[WTBK][HOME]--;
	assert(st->router->egress_credit[WTBK][HOME] >= 0);
	/* now notice L1 we are going to issue replace op */

	if (st->missq[replace_L1i].inst) {
	  replace_block(st->icache,st->irepbuf.way,st->missq[replace_L1i].paddr,NO_REQ,1,0, UNTOUCHED);
	  st->fetch_istall_buf.stall |= REFILL_STALL;
	} else {
	  refill = refill_get_from_free_list(st);
	  /* is replace op */
	  refill->replace = 1;
	  refill->intervention = 0;
	  refill->set = st->missq[replace_L1i].set;
	  refill->paddr = st->missq[replace_L1i].paddr;
#ifdef MESI
	  refill->req = st->missq[replace_L1i].req;
	  refill->cache_status = st->missq[replace_L1i].cache_status;
	  /* search missq for conflict, write here for convience, maybe need another pipeline stage for this */
	  if (st->missq[replace_L1i].req == READ_SHD && refill->cache_status == EXCLUSIVE) {
		st->missq[replace_L1i].req = READ_EX;
		for(i = 0; i < missq_ifq_size; i++){
		  if (i != replace_L1i && st->missq[i].state != MQ_EMPTY && st->missq[i].cpuid == st->missq[replace_L1i].cpuid && BLOCK_MATCH(st->missq[i].paddr, st->missq[replace_L1i].paddr)) {
			if (!((st->missq[i].req == READ_EX || st->missq[i].req == UPGRADE) && st->missq[i].state == MQ_L1_MISS))
			  __asm__("int3");
			st->missq[i].state = MQ_EMPTY;
			st->missq_num--;
			/* update home missq credit */
			eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, REQ, HOME);
		  }
		}
	  }
#endif
	  /* passed to dcache */
	  st->lsq_refill = refill;
	}
	
	missq_change_state(st,replace_L1i,MQ_REFILL_L1);

  } else {

	/* L2 miss occured, need to replace L1 cache block back.*/
	if ( L2_missi != -1 && wtbkq_has_two_empty && !missq_has_delay1){
	  if (st->missq[L2_missi].L2_replace_blk->status != INVALID){
		/* if this INTERVENTION_EXC need to send to local instruction L1, send it here. */
		if(st->missq[L2_missi].inst_directory[st->cpuid] == 1 && st->missq[L2_missi].inst_intervention_sent[st->cpuid] != 1 ){
		  st->wtbkq_num++;
		  assert(st->wtbkq_num <= wtbkq_ifq_size);
		  st->router->egress_credit[WTBK][HOME]--;
		  assert(st->router->egress_credit[WTBK][HOME] >= 0);
		  replace_block(st->icache,st->irepbuf.way,st->missq[L2_missi].L2_replace_paddr,NO_REQ,1,1, UNTOUCHED);
		  st->missq[L2_missi].inst_intervention_sent[st->cpuid] = 1;
		  st->fetch_istall_buf.stall |= REFILL_STALL;
		}else if (st->missq[L2_missi].data_directory[st->cpuid] && !st->missq[L2_missi].data_intervention_sent[st->cpuid] && !st->refill_delay){

		  /* notify the dcache there is an intervention op */

		  refill = refill_get_from_free_list(st);
		  refill->replace = 0;
		  /* is intervention op */
		  refill->intervention = 1;
		  refill->req = st->missq[L2_missi].intervention_type;
		  /* replace L1 which paddr is paddr of L2_replace_blk. */
		  refill->paddr = st->missq[L2_missi].L2_replace_paddr;
		  refill->missqid = L2_missi;
		  st->lsq_refill = refill;
		  st->dcache_refill_access++;

		  st->missq[L2_missi].data_intervention_sent[st->cpuid] = 1;
		}
	  }
	}
  }


  /* make missq delay 3 cycles, silly code, use event? */

  for (i=0;i<missq_ifq_size;i++) {
    if (st->missq[i].state==MQ_DELAY3) {
      st->missq[i].state = MQ_EMPTY;
      st->missq_num--;
      /* update home missq credit */
      eventq_queue_callback4(sim_cycle, update_credit, st->router->home_x, st->router->home_y, REQ, HOME);
    }else if (st->missq[i].state==MQ_DELAY2) {
      st->missq[i].state = MQ_DELAY3;
    }else if (st->missq[i].state==MQ_DELAY1) {
      st->missq[i].state = MQ_DELAY2;
    }
  }


}


/* internal or external writeback or eliminate request enter wtbkq */
void wtbkq_enter(struct godson2_cpu *st, int request_cpuid, int req,md_addr_t paddr,int inst, int cache_status,int intervention)
{
  int i,j;
#if 0
  /* check hit */
  for (i=0;i<wtbkq_ifq_size;i++) {
    if (st->wtbkq[i].cpuid==request_cpuid && st->wtbkq[i].valid && BLOCK_MATCH(paddr,st->wtbkq[i].paddr)) {
      /* merge this request with already existed request */
      st->wtbkq[i].w = (req == WRITEBACK) ? w : st->wtbkq[i].w;
      return;
    }
  }
#endif
  /* no hit */
  /* fifo replacement*/
  for (i=st->wtbkq_replacei,j=0;j<wtbkq_ifq_size;j++) {
    if (!st->wtbkq[i].valid) break;
    i = (i+1) % wtbkq_ifq_size;
  }
  assert(!st->wtbkq[i].valid);
  st->wtbkq[i].valid = 1;
  st->wtbkq[i].cpuid = request_cpuid;
  st->wtbkq[i].req   = req;
  st->wtbkq[i].paddr = paddr;
  st->wtbkq[i].inst  = inst;
  st->wtbkq[i].cache_status = cache_status;
  st->wtbkq[i].intervention = intervention;
  st->wtbkq_replacei = (i+1) % wtbkq_ifq_size;
  st->wtbkq_access++;
  /* wtbkq_num and egress_credit has been updated in advance .*/
  /*
  st->router->egress_credit[WTBK][HOME]--;
  assert(st->router->egress_credit[WTBK][HOME] >= 0);*/
}

/* simple memory simulation */
void check_memq(struct godson2_cpu *st)
{
  int freed;
  struct memory_queue *tmp,*next;;

  if (st->memq_num==0) return;

  st->memq_busy_count ++;

  tmp = st->memq_head;
  st->memq_head = NULL;

  while (tmp) {
    tmp->count++;

    freed = FALSE;
    next = tmp->next;

    if (tmp->read == Read) {
      if (tmp->count >= mem_read_first_delay + st->missq[tmp->qid].memcnt * mem_read_interval_delay) {
	st->missq[tmp->qid].memcnt++;
	if (st->missq[tmp->qid].memcnt == 4) {
	  /* free this item */
	  memq_return_to_free_list(st, tmp);
	  freed = TRUE;
	}
      }
    }else{ /*Write command*/
      if (tmp->count==mem_write_delay) {
	/* free this item */
	memq_return_to_free_list(st, tmp);
	freed = TRUE;
      }
    }
    /* reinsert to queue if not freed */
    if (!freed) {
      tmp->next = st->memq_head;
      st->memq_head  = tmp;
    }
    tmp = next;
  }
//  eventq_queue_callback(sim_cycle+st->period,(void*)check_memq,(int)st);
}

/* dump all missq and wtbkq entry */
void cache2mem_dump(struct godson2_cpu *st)
{
  int i;
  struct memory_queue *tmp;

  myfprintf(stderr,"writeback queue : valid cache_status paddr\n"); 
  for (i=0;i<wtbkq_ifq_size;i++) {
    myfprintf(stderr,"            %5d %1d %8x\n",st->wtbkq[i].valid,st->wtbkq[i].cache_status,st->wtbkq[i].paddr);
  }
  myfprintf(stderr,"miss queue :state set   paddr  inst memcnt cachecnt\n"); 
  for (i=0;i<missq_ifq_size;i++) {
    myfprintf(stderr,"           %5d %3d %8d %4d %6d %6d\n",st->missq[i].state,st->missq[i].set,
	st->missq[i].paddr,st->missq[i].inst,st->missq[i].memcnt,st->missq[i].cachecnt);
  }
  printf("memq has %d item: qid read count\n",st->memq_num);
  tmp = st->memq_head;
  while (tmp) {
    myfprintf(stderr,"         %3d %4d %4d\n", tmp->qid,tmp->read,tmp->count);
    tmp=tmp->next;
  }
}

void missq_change_state(struct godson2_cpu *st,int qid,int new_state){
  if (st->missq[qid].state == MQ_EMPTY){
    /* entering missqueue, update credit. */
    st->router->egress_credit[REQ][HOME]--;
    st->router->egress_credit[INVN][HOME]--;
    assert(st->router->egress_credit[REQ][HOME] >= 0);
    assert(st->router->egress_credit[INVN][HOME] >= 0);
  }
  if (new_state == MQ_EMPTY)
	memset(&(st->missq[qid]), 0, sizeof(struct miss_queue));
  st->lasti[st->missq[qid].state] = qid;
  st->missq[qid].state = new_state;
}
