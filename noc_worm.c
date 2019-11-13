/*
 * noc.c - implementing NOC with Wave Front Allocator
 * (reference: Principles and Practices of Interconnection Networks)
 *
 * This file is part of the godson2 simulator tool suite.
 *
 * Copyright (C) 2004 by Fuxin Zhang, ICT.
 * Copyright (C) 2006 by Huangkun, ICT.
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
#include <signal.h>
#include "mips.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "cache.h"
#include "bpred.h"
#include "tlb.h"
#include "eventq.h"
#include "stats.h"
#include "syscall.h"
#include "sim.h"
#include "godson2_cpu.h"
#include "noc.h"
#include "ptrace.h"
#include "cache.h"

#include "istat.h"
#include "cache2mem.h"

/* home cpu missq receive external request(READ_SHD,READ_EX,UPGRADE) or response(ACK,ACK_DATA,NACK),or wtbkq receive remote wtbk request(WRITEBACK,ELIMINATE),or extinvnq receive coherence intervention(INTERVENTION_SHD,INTERVENTION_EXC,INVALIDATE) */
void home_enter(struct inputbuffer* req_packet, int home_x, int home_y)
{
  assert(req_packet);
  int i, home_cpuid;
  int req         = req_packet->req;
  int L1_status   = req_packet->cache_status;
  int qid         = req_packet->qid;
  int cpuid       = req_packet->cpuid;
  md_addr_t paddr = req_packet->paddr;
  int inst        = req_packet->inst;
  
  assert(req_packet->dest_x == home_x && req_packet->dest_y == home_y);
  home_cpuid = home_x + home_y * mesh_width;
  
  struct cache_blk *blk = cpus[home_cpuid].missq[qid].L2_changed_blk;
  switch(req){
  case READ_SHD:
  case READ_EX:
  case UPGRADE:
	/* remote request enter home missq */
	i = find_missq_two_empty_entry(&cpus[home_cpuid]);
	assert(i!=-1);
	missq_change_state(&cpus[home_cpuid],i,MQ_L1_MISS);
	cpus[home_cpuid].missq[i].req   = req;
	cpus[home_cpuid].missq[i].ack   = NO_REQ;
	cpus[home_cpuid].missq[i].qid   = qid;
	cpus[home_cpuid].missq[i].cpuid = cpuid;
	cpus[home_cpuid].missq[i].paddr = paddr;
	cpus[home_cpuid].missq[i].inst  =  inst;
	cpus[home_cpuid].missq[i].memcnt= 0;
	cpus[home_cpuid].missq[i].cachecnt = 0;
	cpus[home_cpuid].missq[i].wait_for_reissue = 0;
	cpus[home_cpuid].missq[i].wait_for_response = 0;
	cpus[home_cpuid].missq[i].memread_sent = 0;
	cpus[home_cpuid].missq_num++;
	cpus[home_cpuid].missq_access++;
	break;

  case WRITEBACK:
  case ELIMINATE:
	/* remote write request enter home wtbkq */
	cpus[home_cpuid].wtbkq_num++;
	assert(cpus[home_cpuid].wtbkq_num <= wtbkq_ifq_size);
	cpus[home_cpuid].router->egress_credit[WTBK][HOME]--;
	assert(cpus[home_cpuid].router->egress_credit[WTBK][HOME] >= 0);
	wtbkq_enter(&cpus[home_cpuid], cpuid, req, paddr, inst, (req==WRITEBACK)?1:0,0);
	break;

  case INTERVENTION_SHD:
  case INTERVENTION_EXC:
  case INVALIDATE:
	i = find_missq_empty_entry(&cpus[home_cpuid]);
	missq_change_state(&cpus[home_cpuid],i,MQ_MODIFY_L1);
	cpus[home_cpuid].missq[i].modify_l1 = 1;
	cpus[home_cpuid].missq[i].req   = req;
	cpus[home_cpuid].missq[i].ack   = NO_REQ;
	cpus[home_cpuid].missq[i].qid   = qid;
	cpus[home_cpuid].missq[i].cpuid = cpuid;
	cpus[home_cpuid].missq[i].paddr = paddr;
	cpus[home_cpuid].missq[i].inst  = inst;
	cpus[home_cpuid].missq[i].wtbkq_checked = 0;
	cpus[home_cpuid].missq[i].conflict_checked = 0;
	cpus[home_cpuid].missq[i].wait_for_reissue = 0;
	cpus[home_cpuid].missq[i].wait_for_response = 0;
	cpus[home_cpuid].missq[i].memread_sent = 0;
	cpus[home_cpuid].missq_num++;
	cpus[home_cpuid].missq_access++;
	break;

  case ACK:
  case ACK_DATA:
	  
	if (cpus[home_cpuid].missq[qid].state == MQ_MODIFY_L1 && BLOCK_MATCH(cpus[home_cpuid].missq[qid].paddr , paddr)) {

	  /* missq now is modifying other L1s and waiting for acks */
	  /* check cache status in response and decide if requests needs to be re-sent */

	  switch (cpus[home_cpuid].missq[qid].intervention_type){
	  case INTERVENTION_SHD:
		if (L1_status == EXCLUSIVE || L1_status == MODIFIED){/* L2 status must be EXCLUSIVE status here */
		  cpus[home_cpuid].missq[qid].data_ack_received[cpuid] = 1;
		  if (L1_status == MODIFIED)
			cpus[home_cpuid].missq[qid].L2_changed_blk->dirty = 1;
		} else {
		  /* INVALID and SHARED L2 should not send INTERVENTION_SHD request */
		  assert((blk->status != INVALID) && (blk->status != SHARED));
		  cpus[home_cpuid].missq[qid].data_intervention_sent[cpuid] = 0;
		}
		break;
	  case INTERVENTION_EXC:
		if (blk->status == L1_status || (blk->status == EXCLUSIVE && L1_status == MODIFIED)){
		  if (inst)
			cpus[home_cpuid].missq[qid].inst_ack_received[cpuid] = 1;
		  else
			cpus[home_cpuid].missq[qid].data_ack_received[cpuid] = 1;
		  if (L1_status == MODIFIED)
			cpus[home_cpuid].missq[qid].L2_changed_blk->dirty = 1;
		} else {
		  /* L1 cache status should be higher than that of L2 */
		  assert((blk->status != INVALID) && !(blk->status == SHARED && (L1_status == EXCLUSIVE || L1_status == MODIFIED)));
		  if (inst)
			cpus[home_cpuid].missq[qid].inst_intervention_sent[cpuid] = 0;
		  else
			cpus[home_cpuid].missq[qid].data_intervention_sent[cpuid] = 0;
		}
		break;
	  case INVALIDATE:
		if (blk->status == L1_status){/* L2 status must be shared here. */
		  if (inst)
			cpus[home_cpuid].missq[qid].inst_ack_received[cpuid] = 1;
		  else
			cpus[home_cpuid].missq[qid].data_ack_received[cpuid] = 1;
		}
		else{
		  assert((blk->status != INVALID) && (blk->status != EXCLUSIVE) && !(blk->status == SHARED && (L1_status == EXCLUSIVE || L1_status == MODIFIED)));
		  if (inst)
			cpus[home_cpuid].missq[qid].inst_intervention_sent[cpuid] = 0;
		  else
			cpus[home_cpuid].missq[qid].data_intervention_sent[cpuid] = 0;
		}
		break;
	  }
		
	} else if(cpus[home_cpuid].missq[qid].state == MQ_READ_L2 && BLOCK_MATCH(cpus[home_cpuid].missq[qid].paddr , paddr)) {
		
	  /* missq now is waiting for remote refill data */
	  cpus[home_cpuid].missq[qid].memcnt = 4;
#ifdef MESI
	  cpus[home_cpuid].missq[qid].cache_status = L1_status;
#endif
	  missq_change_state(&cpus[home_cpuid],qid,MQ_REPLACE_L1);
		
	} else if (cpus[home_cpuid].missq[qid].state == MQ_L2_MISS && BLOCK_MATCH(cpus[home_cpuid].missq[qid].L2_replace_paddr, paddr)) {
		
	  struct cache_blk* blk = cpus[home_cpuid].missq[qid].L2_replace_blk;
	  assert((blk->status != INVALID) && !(blk->status == SHARED && (L1_status == EXCLUSIVE || L1_status == MODIFIED)));
	  if (blk->status == L1_status || (blk->status == EXCLUSIVE && (L1_status == EXCLUSIVE || L1_status == MODIFIED))) {
		if (inst)
		  cpus[home_cpuid].missq[qid].inst_ack_received[cpuid] = 1;
		else
		  cpus[home_cpuid].missq[qid].data_ack_received[cpuid] = 1;
	  } else {
		if (inst)
		  cpus[home_cpuid].missq[qid].inst_intervention_sent[cpuid] = 0;
		else
		  cpus[home_cpuid].missq[qid].data_intervention_sent[cpuid] = 0;
	  }
	  if (L1_status == MODIFIED)
		blk->dirty = 1;
	}

	break;
	  
  case NACK:
#if 0
	/* if remote processor send a NACK to the directory, that means the cache block status is not consistency. the intervention or invalidate request need to resend. set ack_send array to indicate this condition. */
	if(cpus[home_cpuid].missq[qid].state == MQ_MODIFY_L1){
	  /* missq now is modifying other L1s and waiting for acks */
	  cpus[home_cpuid].missq[qid].ack_received[cpuid] = 1;
	}else if(cpus[home_cpuid].missq[qid].state == MQ_REFILL_L1 ||
			 cpus[home_cpuid].missq[qid].state == MQ_READ_L2){
	  /* missq now is waiting for remote refill data */
	  cpus[home_cpuid].missq[qid].memcnt = 4;
	}
#endif
	break;
  }
}
/*********************************************************************************************/
void crossbar(struct godson2_cpu *st)
{
  /* now nothing to do */
}

/* now router use simple X-Y routing algorithm, round robin arbitrating policy and credit-based flow control algoruthm */
void arbitrate_worm(struct godson2_cpu *st)
{
  int type, i, j, req[NUM_DIRECTIONS+1],conti;

  for(type=0 ; type<NUM_TYPES ; type++) {
    for(i=0 ; i<NUM_DIRECTIONS ; i++){
      st->router->egress_grant[type][i] = NULL;
    }
  }

  /* round robin priority order is :
   * HOME <-> UP <-> DOWN <-> LEFT <-> RIGHT
   *  0       1       2        3         4
   */

  for (type=0 ; type<NUM_TYPES ; type++) {
        conti=1;
        for(i=0;i<NUM_DIRECTIONS;i++)
                if(st->router->ingress_num[type][i]>0) conti=0;
        if(conti)       continue;

    /* home inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    /* external write back should allow the internal write back issue first.*/
    if(st->router->egress_credit[type][HOME] > (type==REQ /*|| type == WTBK*/)?1:0 || type == RESP){
      if(st->router->used[type][HOME]==0)
      {
      for(i=0 ; i<NUM_DIRECTIONS ; i++){
                if(st->router->ingress_num[type][i] > 0)
          if( st->router->ingress[type][i][st->router->ingress_head[type][i]]->flit_type==FLIT_HEAD &&
              st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x == st->router->home_x &&
              st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_y == st->router->home_y)
                        req[i] = 1;
          }

          for(i=1 ; i<=NUM_DIRECTIONS ; i++){
            j = (st->router->egress_last_grant[type][HOME] + i) % NUM_DIRECTIONS;
            if(req[j] == 1){
                        st->router->egress_grant[type][HOME] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
                        st->router->egress_last_grant[type][HOME] = j;
                        st->router->inputdirec[type][HOME]=j;
 		        if(PACK_SIZE>1)
                        st->router->used[type][HOME]=1;
                  break;
            }
          }
        }else{
          int dir=st->router->inputdirec[type][HOME];
          if(dir<0) fatal("input direc wrong\n");
          if(st->router->ingress_num[type][dir]>0){
                struct inputbuffer *packet=st->router->ingress[type][dir][st->router->ingress_head[type][dir]];
                st->router->egress_last_grant[type][HOME]=dir;
                st->router->egress_grant[type][HOME]=packet;
                if(packet->flit_type==FLIT_TAIL){
                        st->router->inputdirec[type][HOME]=-1;
                        st->router->used[type][HOME]=0;
                }
          }
        }
        }

    /* up inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][UP] > 0){
        if(st->router->used[type][UP]==0)
        {
          for(i=0 ; i<NUM_DIRECTIONS ; i++){
                if(st->router->ingress_num[type][i] > 0)
          if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->flit_type==FLIT_HEAD &&
                st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x == st->router->home_x &&
                 st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_y < st->router->home_y)
                          req[i] = 1;
          }

          for(i=1 ; i<=NUM_DIRECTIONS ; i++){
            j = (st->router->egress_last_grant[type][UP] + i) % NUM_DIRECTIONS;
            if(req[j] == 1){
                  st->router->egress_grant[type][UP] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
                  st->router->egress_last_grant[type][UP] = j;
                  st->router->inputdirec[type][UP]=j;
		  if(PACK_SIZE>1)
                  st->router->used[type][UP]=1;
                  break;
            }
          }
        }else{
          int dir=st->router->inputdirec[type][UP];
          if(dir<0) fatal("input direc wrong\n");
          if(st->router->ingress_num[type][dir]>0){
                struct inputbuffer *packet=st->router->ingress[type][dir][st->router->ingress_head[type][dir]];
                st->router->egress_last_grant[type][UP]=dir;
                st->router->egress_grant[type][UP]=packet;
                if(packet->flit_type==FLIT_TAIL){
                        st->router->inputdirec[type][UP]=-1;
                        st->router->used[type][UP]=0;
                }
          }
        }
        }

    /* down inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][DOWN] > 0){
        if(st->router->used[type][DOWN]==0)
        {           for(i=0 ; i<NUM_DIRECTIONS ; i++){
                if (st->router->ingress_num[type][i] > 0)
          if( st->router->ingress[type][i][st->router->ingress_head[type][i]]->flit_type==FLIT_HEAD &&
                st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x == st->router->home_x &&
                 st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_y > st->router->home_y)
                          req[i] = 1;
          }

          for(i=1 ; i<=NUM_DIRECTIONS ; i++){
            j = (st->router->egress_last_grant[type][DOWN] + i) % NUM_DIRECTIONS;
            if(req[j] == 1){
                  st->router->egress_grant[type][DOWN] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
                  st->router->egress_last_grant[type][DOWN] = j;
                  st->router->inputdirec[type][DOWN]=j;
		  if(PACK_SIZE>1)
                  st->router->used[type][DOWN]=1;
                  break;
            }
          }
        }else{
          int dir=st->router->inputdirec[type][DOWN];
          if(dir<0) fatal("input direc wrong\n");
          if(st->router->ingress_num[type][dir]>0){
                struct inputbuffer *packet=st->router->ingress[type][dir][st->router->ingress_head[type][dir]];
                st->router->egress_last_grant[type][DOWN]=dir;
                st->router->egress_grant[type][DOWN]=packet;
                if(packet->flit_type==FLIT_TAIL){
                        st->router->inputdirec[type][DOWN]=-1;
                        st->router->used[type][DOWN]=0;
                }
          }
        }
    }

    /* left inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][LEFT] > 0){
        if(st->router->used[type][LEFT]==0)
        {
          for(i=0 ; i<NUM_DIRECTIONS ; i++){                 if(st->router->ingress_num[type][i] > 0)
          if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->flit_type==FLIT_HEAD &&
                st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x < st->router->home_x)
                        req[i] = 1;
          }

          for(i=1 ; i<=NUM_DIRECTIONS ; i++){
            j = (st->router->egress_last_grant[type][LEFT] + i) % NUM_DIRECTIONS;
            if(req[j] == 1){
                  st->router->egress_grant[type][LEFT] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
                  st->router->egress_last_grant[type][LEFT]  = j;
                  st->router->inputdirec[type][LEFT]=j;
		  if(PACK_SIZE>1)
                  st->router->used[type][LEFT]=1;
                  break;
            }
          }
        }else{
          int dir=st->router->inputdirec[type][LEFT];
          if(dir<0) fatal("input direc wrong\n");
          if(st->router->ingress_num[type][dir]>0){
                struct inputbuffer *packet=st->router->ingress[type][dir][st->router->ingress_head[type][dir]];
                st->router->egress_last_grant[type][LEFT]=dir;
                st->router->egress_grant[type][LEFT]=packet;
                if(packet->flit_type==FLIT_TAIL){
                        st->router->inputdirec[type][LEFT]=-1;
                        st->router->used[type][LEFT]=0;
                }
          }
        }
        }

    /* right inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][RIGHT] > 0){
        if(st->router->used[type][RIGHT]==0)
        {
          for(i=0 ; i<NUM_DIRECTIONS ; i++){
                if(st->router->ingress_num[type][i] > 0)
          if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x > st->router->home_x)
                        req[i] = 1;
          }

          for(i=1 ; i<=NUM_DIRECTIONS ; i++){
            j = (st->router->egress_last_grant[type][RIGHT] + i) % NUM_DIRECTIONS;
            if(req[j] == 1){
                  st->router->egress_grant[type][RIGHT] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
                  st->router->egress_last_grant[type][RIGHT] = j;
                  st->router->inputdirec[type][RIGHT]=j;
		  if(PACK_SIZE>1)
                  st->router->used[type][RIGHT]=1;
                  break;
            }
          }
        }else{
          int dir=st->router->inputdirec[type][RIGHT];
          if(dir<0) fatal("input direc wrong\n");
          if(st->router->ingress_num[type][dir]>0){
                struct inputbuffer *packet=st->router->ingress[type][dir][st->router->ingress_head[type][dir]];
                st->router->egress_last_grant[type][RIGHT]=dir;
                st->router->egress_grant[type][RIGHT]=packet;
                if(packet->flit_type==FLIT_TAIL){
                        st->router->inputdirec[type][RIGHT]=-1;
                        st->router->used[type][RIGHT]=0;
                }
          }
        }
    }
  }/* end of channel type loop */
}

/*********************************************************************************************/


void update_credit(tick_t now, int home_x, int home_y, int type, int direction)
{
  int home_cpuid = home_x + home_y * mesh_width;
  if (direction == HOME){
    if (type == REQ || type == INVN){
      cpus[home_cpuid].router->egress_credit[REQ][HOME]++;
      cpus[home_cpuid].router->egress_credit[INVN][HOME]++;
      assert(cpus[home_cpuid].router->egress_credit[REQ][HOME] <= missq_ifq_size);
      assert(cpus[home_cpuid].router->egress_credit[INVN][HOME] <= missq_ifq_size);
    } else if (type == WTBK){
      cpus[home_cpuid].router->egress_credit[WTBK][HOME]++;
      assert(cpus[home_cpuid].router->egress_credit[WTBK][HOME] <= wtbkq_ifq_size);
    }
  } else {
    cpus[home_cpuid].router->egress_credit[type][direction]++;
    assert(cpus[home_cpuid].router->egress_credit[type][direction] <= router_ifq_size);
  }
}

extern void flit_traverse(struct godson2_cpu *st);

/* network on chip access handler */
void noc_stage(struct godson2_cpu *st)
{
  flit_traverse(st);
  crossbar(st);
  arbitrate_worm(st);
}

void noc_init()
{
  int i, j, k;

  for (k = 0; k < total_cpus; k++) {
	cpus[k].router = calloc(1, sizeof(struct router_t));
	if (!cpus[k].router)
	  fatal("out of memory when noc_init\n");

	for(i=0 ; i<NUM_TYPES ; i++){
	  for(j=0 ; j<NUM_DIRECTIONS ; j++){
		cpus[k].router->ingress[i][j] = (struct inputbuffer **)calloc(router_ifq_size, sizeof(struct inputbuffer*));
		memset(cpus[k].router->ingress[i][j], 0, router_ifq_size * sizeof(struct inputbuffer*));
		cpus[k].router->ingress_head[i][j] = cpus[k].router->ingress_tail[i][j] = cpus[k].router->ingress_num[i][j] = 0;
		if (j == HOME){
		  if (i == REQ || i == INVN)
			cpus[k].router->egress_credit[i][j] = missq_ifq_size;
		  else if (i == WTBK)
			cpus[k].router->egress_credit[i][j] = wtbkq_ifq_size;
		  else
			cpus[k].router->egress_credit[i][j] = 1; /* RESP always have their positions. */
		}
		else
		  cpus[k].router->egress_credit[i][j] = router_ifq_size;
		cpus[k].router->egress_grant[i][j] = NULL;
		cpus[k].router->egress_last_grant[i][j] = NOUGHT;
		cpus[k].router->used[i][j]=0;
		cpus[k].router->inputdirec[i][j]=-1;
	  }
	}

	cpus[k].router->home_x = k % mesh_width;
	cpus[k].router->home_y = k / mesh_width;
  }

  mesh_height = total_cpus / mesh_width;
}

/* network on chip request */
int noc_request(struct godson2_cpu *st, int chan_type, int request,int cache_status, int qid, int cpuid, md_addr_t paddr, int inst)
{
  if (st->router->ingress_num[chan_type][HOME] >(router_ifq_size-PACK_SIZE))  return 0;
  
  struct inputbuffer **home = st->router->ingress[chan_type][HOME];
  struct inputbuffer *packet[PACK_SIZE];
  int head = st->router->ingress_head[chan_type][HOME];
  int pos = 0, i, dest_cpuid,j;

  for(j=0;j<PACK_SIZE;j++) { packet[j]=NULL;
  packet[j] = (struct inputbuffer *)malloc(sizeof(struct inputbuffer));
  if (packet[j] == NULL)
	fatal("out of memory in noc_request\n");
  memset(packet[j], 0, sizeof(struct inputbuffer));
  packet[j]->flit_type=FLIT_BODY;
  }
  packet[0]->flit_type=FLIT_HEAD;
  if(PACK_SIZE>1)
  packet[PACK_SIZE-1]->flit_type=FLIT_TAIL;

  if(chan_type==INVN || chan_type==RESP){
    dest_cpuid = cpuid;/* destination cpuid */
  }else{
    dest_cpuid = PADDR_OWNER(paddr);/* paddr decide the destination cpuid which the request initiator want to access */
  }
  for(j=0;j<PACK_SIZE;j++)
  for (i = 0; i < router_ifq_size; i++) {
	pos = (head + i) % router_ifq_size;
	if (!home[pos]) {
	  packet[j]->valid = 1;
	  packet[j]->source_channel = HOME;
	  packet[j]->cache_status   = cache_status;
	  packet[j]->index  = pos;
	  packet[j]->dest_x = dest_cpuid % mesh_width;
	  packet[j]->dest_y = dest_cpuid / mesh_width;
	  packet[j]->cpuid  = st->cpuid;
	  packet[j]->req    = request;
	  packet[j]->qid    = qid;
	  packet[j]->paddr  = paddr;
	  packet[j]->inst   = inst;
	  packet[j]->time_stamp = sim_cycle;
	  home[pos] = packet[j];
	  break;
	}
  }
  
  st->router->pack_sent[chan_type]++;
  for(i=0;i<PACK_SIZE;i++){
    st->router->ingress_num[chan_type][HOME]++;
    st->router->input_buffer_write_access[chan_type][HOME]++;
  }
  assert(st->router->ingress_num[chan_type][HOME] <= router_ifq_size);

  //assert(pos == st->router->ingress_tail[chan_type][HOME]);
  //st->router->ingress_tail[chan_type][HOME] = (st->router->ingress_tail[chan_type][HOME] + 1) % router_ifq_size;
  return 1;
}

void flit_arrive(tick_t now, struct inputbuffer* packet, int home_x, int home_y, int type, int direction)
{
  int home_cpuid = home_x + home_y * mesh_width;
  struct inputbuffer **in;
  int i, head, pos = 0;
  
  in = cpus[home_cpuid].router->ingress[type][direction];
  head  = cpus[home_cpuid].router->ingress_head[type][direction];

  cpus[home_cpuid].router->input_buffer_write_access[type][direction]++;

  for (i = 0; i < router_ifq_size; i++) {
	pos = (head + i) % router_ifq_size;
	if (!in[pos]) {
	  in[pos] = packet;
	  packet->valid = 1;
	  packet->source_channel = direction;
	  packet->index = pos;
	  break;
	}
  }

  //assert(pos == cpus[home_cpuid].router->ingress_tail[type][direction]);
  //cpus[home_cpuid].router->ingress_tail[type][direction] = 
  //(cpus[home_cpuid].router->ingress_tail[type][direction] + 1) % router_ifq_size;

  cpus[home_cpuid].router->ingress_num[type][direction]++;
#ifndef NDEBUG 
  if (cpus[home_cpuid].router->ingress_num[type][direction] > router_ifq_size){
    printf("input buffer overflow at physical channel %d of %d direction in router of cpu %d !\n",type,direction,home_cpuid);
  }
#endif
  assert(cpus[home_cpuid].router->ingress_num[type][direction] <= router_ifq_size);
}

int wire_delay = 1;
static void update_ingress_head(struct router_t *router, int type, int source_channel)
{
  int head, pos = 0, i;
  head = router->ingress_head[type][source_channel];
  for (i = 0; i < router_ifq_size; i++) {
	pos = (head + i) % router_ifq_size;
	if (router->ingress[type][source_channel][pos] != NULL) {
	  router->ingress_head[type][source_channel] = pos;
	  break;
	}
  }
  //if (i == router_ifq_size)
  //router->ingress_head[type][source_channel] = (head + 1) % router_ifq_size;
}

static void packet_depart(struct router_t *router, struct inputbuffer *req_packet, int type, int out_direct)
{
  int source_channel = req_packet->source_channel;
  int index          = req_packet->index;

  req_packet->valid = 0;
  router->ingress[type][source_channel][index] = NULL;
  //head = router->ingress_head[type][source_channel];
  update_ingress_head(router, type, source_channel);
  //assert(router->ingress_head[type][source_channel] == (head+1) % router_ifq_size); 

  router->ingress_num[type][source_channel]--;
  if (out_direct != HOME)
	router->egress_credit[type][out_direct]--;
  assert(router->ingress_num[type][source_channel] >= 0);
  assert(router->ingress_num[type][source_channel] <= router_ifq_size);
  assert(router->egress_credit[type][out_direct] >= 0);
}

#define SEND_CREDIT_UP																		\
  case UP:																					\
	assert (st->router->home_y - 1 >= 0);													\
  	  eventq_queue_callback4(sim_cycle + wire_delay, update_credit,							\
							 st->router->home_x, st->router->home_y-1, type, DOWN);			\
	break;

#define SEND_CREDIT_DOWN																	\
  case DOWN:																				\
	assert (st->router->home_y + 1 < mesh_height);											\
	  eventq_queue_callback4(sim_cycle + wire_delay, update_credit,							\
							 st->router->home_x, st->router->home_y+1, type, UP);			\
	break;

#define SEND_CREDIT_LEFT																	\
  case LEFT:																				\
	assert (st->router->home_x - 1 >= 0);													\
	  eventq_queue_callback4(sim_cycle + wire_delay, update_credit,							\
							 st->router->home_x-1, st->router->home_y, type, RIGHT);		\
	break;

#define SEND_CREDIT_RIGHT																	\
  case RIGHT:																				\
	assert (st->router->home_x + 1 < mesh_width);											\
	  eventq_queue_callback4(sim_cycle + wire_delay, update_credit,							\
							 st->router->home_x+1, st->router->home_y, type, LEFT);			\
	break;

/* send ready flit and credit infomation */
void flit_traverse(struct godson2_cpu *st)
{
  int type,ingress_head;
  struct inputbuffer *req_packet;

  for(type = 0 ; type < NUM_TYPES ; type++){
	/* send ready flit into home, RESP need not credit to enter home missq */
	if(st->router->egress_grant[type][HOME] != NULL){
	  req_packet = st->router->egress_grant[type][HOME];
	  ingress_head=st->router->ingress_head[type][req_packet->source_channel];
	  if(req_packet->index!=ingress_head){
                int secp=(ingress_head+1)%router_ifq_size;
                struct inputbuffer * just=st->router->ingress[type][req_packet->source_channel][ingress_head];
                just->index=secp;
                st->router->ingress[type][req_packet->source_channel][secp]=just;
		req_packet->index=ingress_head;
		st->router->ingress[type][req_packet->source_channel][ingress_head]=req_packet;
         }
	  if(req_packet->flit_type==FLIT_TAIL || PACK_SIZE==1)
	  home_enter(req_packet, st->router->home_x, st->router->home_y);
	  packet_depart(st->router, req_packet, type, HOME);
	  if(req_packet->flit_type==FLIT_TAIL || PACK_SIZE==1)
	  cpus[req_packet->cpuid].router->pack_latency[type] += sim_cycle - req_packet->time_stamp;

	  st->router->crossbar_access[type]++;
	  st->router->input_buffer_read_access[type][req_packet->source_channel]++;
	  
	  switch (req_packet->source_channel){
		SEND_CREDIT_UP
		SEND_CREDIT_DOWN
		SEND_CREDIT_LEFT
		SEND_CREDIT_RIGHT
	  }
	  free(req_packet);
	  /* missq and wtbkq don't need credit, they use ingress_num as credit! */
	}

	/* up direction egress send flit */ 
	if(st->router->egress_grant[type][UP] != NULL && st->router->home_y>0){
	  req_packet = st->router->egress_grant[type][UP];
	  ingress_head=st->router->ingress_head[type][req_packet->source_channel];
	  if(req_packet->index!=ingress_head){
                int secp=(ingress_head+1)%router_ifq_size;
                struct inputbuffer * just=st->router->ingress[type][req_packet->source_channel][ingress_head];
                just->index=secp;
                st->router->ingress[type][req_packet->source_channel][secp]=just;
		req_packet->index=ingress_head;
		st->router->ingress[type][req_packet->source_channel][ingress_head]=req_packet;
         }
	  eventq_queue_callbackpointer4(sim_cycle + wire_delay, flit_arrive, req_packet,
									st->router->home_x, st->router->home_y-1, type, DOWN);
	  packet_depart(st->router, req_packet, type, UP);

	  st->router->crossbar_access[type]++;
	  st->router->input_buffer_read_access[type][req_packet->source_channel]++;

	  /* update credit for upper level*/
	  switch (req_packet->source_channel){
		SEND_CREDIT_DOWN
		SEND_CREDIT_LEFT
		SEND_CREDIT_RIGHT
	  }
	}

	/* down direction egress send flit */
	if(st->router->egress_grant[type][DOWN] != NULL && st->router->home_y<mesh_height){
	  req_packet = st->router->egress_grant[type][DOWN];
	  ingress_head=st->router->ingress_head[type][req_packet->source_channel];
	  if(req_packet->index!=ingress_head){
                int secp=(ingress_head+1)%router_ifq_size;
                struct inputbuffer * just=st->router->ingress[type][req_packet->source_channel][ingress_head];
                just->index=secp;
                st->router->ingress[type][req_packet->source_channel][secp]=just;
		req_packet->index=ingress_head;
		st->router->ingress[type][req_packet->source_channel][ingress_head]=req_packet;
         }
	  eventq_queue_callbackpointer4(sim_cycle + wire_delay, flit_arrive, req_packet,
									st->router->home_x, st->router->home_y+1, type, UP);
	  packet_depart(st->router, req_packet, type, DOWN);

	  st->router->crossbar_access[type]++;
	  st->router->input_buffer_read_access[type][req_packet->source_channel]++;

	  /* update credit for upper level*/
	  switch (req_packet->source_channel){
		SEND_CREDIT_UP
		SEND_CREDIT_LEFT
		SEND_CREDIT_RIGHT
	  }
	}

	/* left direction egress send flit */
	if(st->router->egress_grant[type][LEFT] != NULL && st->router->home_x>0){
	  req_packet = st->router->egress_grant[type][LEFT];
	  ingress_head=st->router->ingress_head[type][req_packet->source_channel];
	  if(req_packet->index!=ingress_head){
                int secp=(ingress_head+1)%router_ifq_size;
                struct inputbuffer * just=st->router->ingress[type][req_packet->source_channel][ingress_head];
                just->index=secp;
                st->router->ingress[type][req_packet->source_channel][secp]=just;
		req_packet->index=ingress_head;
		st->router->ingress[type][req_packet->source_channel][ingress_head]=req_packet;
         }
	  eventq_queue_callbackpointer4(sim_cycle + wire_delay, flit_arrive, req_packet,
									st->router->home_x-1, st->router->home_y, type, RIGHT);
	  packet_depart(st->router, req_packet, type, LEFT);

	  st->router->crossbar_access[type]++;
	  st->router->input_buffer_read_access[type][req_packet->source_channel]++;

	  /* update credit for upper level*/
	  switch (req_packet->source_channel){
		SEND_CREDIT_UP
		SEND_CREDIT_DOWN
		SEND_CREDIT_RIGHT
	  }
	}

	/* right direction egress send flit */
	if(st->router->egress_grant[type][RIGHT] != NULL && st->router->home_x<mesh_width){
	  req_packet = st->router->egress_grant[type][RIGHT];
	  ingress_head=st->router->ingress_head[type][req_packet->source_channel];
	  if(req_packet->index!=ingress_head){
                int secp=(ingress_head+1)%router_ifq_size;
                struct inputbuffer * just=st->router->ingress[type][req_packet->source_channel][ingress_head];
                just->index=secp;
                st->router->ingress[type][req_packet->source_channel][secp]=just;
		req_packet->index=ingress_head;
		st->router->ingress[type][req_packet->source_channel][ingress_head]=req_packet;
         }
	  eventq_queue_callbackpointer4(sim_cycle + wire_delay, flit_arrive, req_packet,
									st->router->home_x+1, st->router->home_y, type, LEFT);
	  packet_depart(st->router, req_packet, type, RIGHT);
	  
	  st->router->crossbar_access[type]++;
	  st->router->input_buffer_read_access[type][req_packet->source_channel]++;

	  /* update credit for upper level*/
	  switch (req_packet->source_channel){
		SEND_CREDIT_UP
		SEND_CREDIT_DOWN
		SEND_CREDIT_LEFT
	  }
	}
  }/* end of channel type loop */
}
