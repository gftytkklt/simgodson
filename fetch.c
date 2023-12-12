/*
* fetch.c - fetch stage implementation
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
#include "cache2mem.h"
#include "ptrace.h"

#include "istat.h"

/*This file implements the fetch stage of the pipeline. */

#if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
/* pipetrace instruction sequence counter */
static unsigned int ptrace_seq = 0;
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

#ifdef ISTAT
tick_t last_fetch_time_stamp = 0;
#endif

int is_split_op(int op)
{
  return (op==MULT || op==MULTU || op==DIV || op==DIVU);
}


/*Initialize the fetch stage*/
void 
fetch_stage_init(struct godson2_cpu *st)
{
  st->fetch_data = (struct inst_descript **)
    calloc(fetch_ifq_size, sizeof(struct inst_descript*));
  
  if (!st->fetch_data) 
    fatal ("out of virtual memory");
  
  st->fetch_num = 0;
  st->fetch_head = 0;
  st->fetch_tail = 0;
  st->fetch_istall_buf.stall=0;
  st->fetch_reg_PC = st->ld_prog_entry;
  st->regs.regs_PC = st->ld_prog_entry;
  st->regs.regs_NPC = st->regs.regs_PC + sizeof(md_inst_t);
  
  /* create the instruction free list */
  fetch_init_inst_free_list(st);

  st->irepbuf.state = IREP_EMPTY;
  st->irepbuf.set = 0xffffffff;
  st->irepbuf.way = 0;
  memset(st->irepbuf.bitmap,0,sizeof(st->irepbuf.bitmap));
}

static int last_inst_missed = FALSE;
static int last_inst_tmissed = FALSE;

counter_t fetch_imiss_cycle,fetch_stall_cycle;

/*fetch instructions into fetch queue*/
void 
fetch_stage(struct godson2_cpu *st)
{
  int lat;
  md_inst_t inst;
  int i=0; 
  int first = TRUE; 
  int within_cache_line = TRUE;
  md_addr_t cache_addr;
  struct bpred_update_t dir_update;
  md_addr_t pred_PC;
  struct inst_descript *current;

  /* somebody want us to rest one cycle */
  if (st->fetch_istall_buf.stall) {
    if ((st->fetch_istall_buf.stall & CACHE_STALL) && st->irepbuf.state == IREP_EMPTY) {
      st->irepbuf.state = IREP_MISS; /* this can be cancel,while IREP_REFILL cannot */
    }
    st->fetch_istall_buf.stall = 0;
    fetch_stall_cycle++; 
    return;
  }

#if 0
  /* TODO: hit under miss */
  if (icache && irepbuf.state!=IREP_EMPTY)
  {
    fetch_imiss_cycle++; 
    return;
  }
#endif

  /* Stop fetch if fetch queue cannot hold fetch_width number of
     instructions. if any instruction is left in ir, godson2 won't
     accept new instructions into it
   */
  if ((st->fetch_num + fetch_width) > fetch_ifq_size) {
    fetch_stall_cycle++; 
    return;
  }

  
  for (i=0; 
       /* fetch bandwidth available */
      i < (fetch_width * fetch_speed) && 
       /* queue not full */
      (st->fetch_num < fetch_ifq_size)  &&
       /* don't fetch beyond cache line boundary */
      within_cache_line ;
      i++){
    
    // myfprintf(stderr,"fetch %x\n",st->fetch_reg_PC);
    /* If valid address */
    // myfprintf(stderr,"PC at %x, valid addr [%x, %x)\n",st->fetch_reg_PC, st->ld_text_base, (st->ld_text_base+st->ld_text_size));
    // myfprintf(stderr,"alignment case is %d\n", !(st->fetch_reg_PC & (sizeof(md_inst_t)-1)));
    if(st->ld_text_base <= st->fetch_reg_PC && 
       st->fetch_reg_PC < (st->ld_text_base+st->ld_text_size) &&  
       !(st->fetch_reg_PC & (sizeof(md_inst_t)-1))){

      /* TODO: add tlb */
      if (st->icache && first){
    	md_addr_t paddr;
    	int tlb_hit;
	    struct cache_blk *blk;

    	tlb_hit = itlb_probe(st->cpuid,st->fetch_reg_PC,&paddr);
    	if (!tlb_hit) {
	      st->fetch_istall_buf.stall |= TLB_STALL;
    	  return;
    	}

	/* godson2 read cache at fetch,detect miss at decode
	 * here if fetch notices a miss,we just record it
	 * then try to issue the miss next cycle. if a miss
	 * is pending,no more misses can be issued.
	 *
	 * But godson2 support hit under miss. While a miss is
	 * pending,the fetch pc can change and hit in the icache,
	 * thus give a valid output.
	 */
    	if (st->irepbuf.state==IREP_EMPTY || CACHE_SET(st->icache,st->fetch_reg_PC) != st->irepbuf.set) {
	  int request = 0;
          blk = cache_probe(st->icache,0,st->fetch_reg_PC,paddr,&request);

          /* add access counters for our power model */
          st->icache_fetch_access++;
    
    	  if (!blk) {
            if (st->irepbuf.state==IREP_EMPTY) {
	      /* cache2mem will check this,accept the miss if it can,
	       * then change the state to IREP_REFILL */
	      //irepbuf.state = IREP_MISS; /* this can be cancel,while IREP_REFILL cannot */
	          if (CACHE_SET(st->icache,st->fetch_reg_PC) == st->irepbuf.set) {
        		/* avoid replacing just refilled block */
        		st->irepbuf.way = (st->irepbuf.way + 1) % st->icache->assoc;
      	      } else {
	        	/* random */
        		st->irepbuf.way = (int)sim_cycle % st->icache->assoc;
    	      }
	          st->irepbuf.set = CACHE_SET(st->icache,st->fetch_reg_PC);
	          st->irepbuf.paddr = paddr;
	          st->irepbuf.bitmap[0] = st->irepbuf.bitmap[1] = st->irepbuf.bitmap[2] = st->irepbuf.bitmap[3] = 0;
	          st->fetch_istall_buf.stall |= CACHE_STALL;
	        } /* else another miss, cannot handle it */
	    /* godson2 detect cache miss at decode stage,so we stop fetch at next cycle 
	     * irepbuf.state is change at that time too
	     */
	        last_inst_missed = TRUE;
	        fetch_imiss_cycle++; 
	        return;
	      } 
	    }else{
	      /* partial hit? */
    	  int k,offset=st->fetch_reg_PC & 0x1f,enough=1;

    	  for (k=0;((k+(offset>>3))<8) && (k<4);k++)
    	    enough = enough && (st->irepbuf.bitmap[k+(offset>>3)]);
    	  if (!enough) return;

        }

      }
      /* We have an instruction from memory. Assign a descriptor for
	 it */
      current = fetch_get_from_free_list(st);
      /* read inst from memory */
      MD_FETCH_INST(inst,st->mem,st->fetch_reg_PC);
      current->trap = FALSE;
    } else { /* invalid pc range */
      // myfprintf(stderr,"bogus inst at %x\n",st->fetch_reg_PC);
      /* BOGUS inst. Send a NOP */
      current = fetch_get_from_free_list(st);
      /* fetch PC is bogus, send a NOP down the pipeline */
      inst = MD_NOP_INST;
      current->trap = TRUE;
    }

    /* Set the opcode */
    MD_SET_OPCODE(current->op,inst);

    if (is_split_op(current->op) && st->fetch_num == fetch_width-1) {
      fetch_return_to_free_list(st,current);
      break;
    }


    /* more to initialize? */
    current->mapped = FALSE;
    current->queued = FALSE;
    current->next_issue = FALSE;
    current->issued = FALSE;
    current->completed = FALSE;
    current->brcompleted = FALSE;
    /* Copy inst value, PC, and the cycle this inst is fetched */ 
    current->IR = inst;
    current->regs_PC = st->fetch_reg_PC;
    current->regs_NPC = st->fetch_reg_PC + sizeof(md_inst_t);
    current->time_stamp = sim_cycle;
#ifdef ISTAT
    current->fetch_latency = sim_cycle - last_fetch_time_stamp;
    current->icache_miss = last_inst_missed;
    current->dcache_miss = 0;
#endif
    current->seq = st->inst_seq++;
    current->ptrace_seq = st->ptrace_seq++;
    current->bht_op = FALSE;
    current->jr_op = FALSE;
    current->bd = FALSE;
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
    /* for pipe trace */
    ptrace_newinst(current->ptrace_seq,
	inst, current->regs_PC,
	0);
    ptrace_newstage(current->ptrace_seq,
	PST_IFETCH, 
	((last_inst_missed ? PEV_CACHEMISS : 0)
	 | (last_inst_tmissed ? PEV_TLBMISS : 0)));
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	
    last_inst_missed = FALSE;
    last_inst_tmissed = FALSE;


    /* if end of cache line is met, stop fetch */
    if (((st->fetch_reg_PC+sizeof(md_inst_t)) % 32) == 0 ) {	
      within_cache_line = FALSE;
    }

    if (first && !pred_perfect){
      pred_PC = bpred_lookup_godson(st->pred,	/* branch predictor instance */
             	  			    st->fetch_reg_PC,
	                            &dir_update);
      /* add access counters for our power model */
      st->bht_fetch_access++;
      st->btb_fetch_access++;
    }

    /* default pred_PC is the next pc */
    current->pred_PC = st->fetch_reg_PC + sizeof(md_inst_t);
    if (MD_OP_FLAGS(current->op) & F_CTRL) { 
      current->dir_update = dir_update;
      if (st->pred->class == BPred2Level) {
    	/* important!! the counter can change between read & update,remember the read value */
    	current->dir_update.st = *dir_update.pdir1;
    	current->pred_taken = (current->dir_update.st>=0x2);
      }else if (st->pred->class == BPredNotTaken){
    	current->pred_taken = 0;
      }

      if (current->op == JR ||
          current->op == JALR) { 
    	current->jr_op = TRUE;
    	/* use btb only for jr */
        current->pred_PC = pred_PC;
#if 0
	printf("predicting %x for %x\n",pred_PC,fetch_reg_PC);
#endif
      }
      if (((MD_OP_FLAGS(current->op) & F_COND) == F_COND) &&
          !(MD_OP_FLAGS(current->op) & F_CTRL_LIKELY) ) {
    	current->bht_op = TRUE;
      }
    }
    /* point to next item,only for 2level presently */
    dir_update.pdir1++;


    st->fetch_data[st->fetch_tail] = current;
    st->fetch_tail = (st->fetch_tail + 1) & (fetch_ifq_size - 1);
    st->fetch_num ++;
    if (is_split_op(current->op)) {
      struct inst_descript *ori = current;
      current = fetch_get_from_free_list(st);

      current->trap = FALSE;
      current->mapped = FALSE;
      current->queued = FALSE;
      current->next_issue = FALSE;
      current->issued = FALSE;
      current->completed = FALSE;
      current->brcompleted = FALSE;
      /* Copy inst value, PC, and the cycle this inst is fetched */ 
      current->IR = inst;
      current->regs_PC  = st->fetch_reg_PC;
      current->regs_NPC = st->fetch_reg_PC + sizeof(md_inst_t);
      current->pred_PC  = st->fetch_reg_PC + sizeof(md_inst_t);
      current->time_stamp = sim_cycle;
#ifdef ISTAT
      current->fetch_latency = sim_cycle - last_fetch_time_stamp;
      current->icache_miss = last_inst_missed;
      current->dcache_miss = 0;
#endif
      current->seq = st->inst_seq++;
      current->ptrace_seq = st->ptrace_seq++;
      current->bht_op = FALSE;
      current->jr_op = FALSE;
      current->bd = FALSE;

      if (ori->op==MULT || ori->op==MULTU) {
        current->op = SPLITMUL;
      }else
        current->op = SPLITDIV;
      st->fetch_data[st->fetch_tail] = current;
      st->fetch_tail = (st->fetch_tail + 1) & (fetch_ifq_size - 1);
      st->fetch_num ++;
    }

    st->fetch_reg_PC += sizeof(md_inst_t);

    st->sim_fetch_insn ++;

    first = FALSE;

	//printf("fetch_reg_PC is 0x%x\n", st->fetch_reg_PC);
  }

#ifdef ISTAT
  last_fetch_time_stamp = sim_cycle;
#endif

  //myfprintf(stderr,"fetch end---\n");
}

/* Initialize the instruction free list */
void fetch_init_inst_free_list(struct godson2_cpu *st)
{
  int i;
  struct inst_descript *temp;
  st->fetch_inst_free_list = NULL;
  warn("Increasing instruction descriptor free list size");
  for (i=0; i<FETCH_FREE_LIST_SIZE; i++){
    temp = (struct inst_descript *) 
      malloc(sizeof(struct inst_descript));
    
    if(!temp) 
      fatal("out of virtual memory");
    temp->next = st->fetch_inst_free_list;
    st->fetch_inst_free_list = temp;

    if (i==0) st->fetch_inst_free_tail = temp;
  }
}  

/* get an entry from free list */
struct inst_descript *
fetch_get_from_free_list(struct godson2_cpu *st)
{
  struct inst_descript *temp;
  
  /* if free list is exhausted, add some more entries */
  if (!st->fetch_inst_free_list){
    fetch_init_inst_free_list(st);
  }
  temp = st->fetch_inst_free_list;
  st->fetch_inst_free_list = st->fetch_inst_free_list->next;
  return (temp);
}

/* return an inst to the free list */
void fetch_return_to_free_list(struct godson2_cpu *st,struct inst_descript *temp)
{
  temp->next = NULL;
  st->fetch_inst_free_tail->next = temp;
  st->fetch_inst_free_tail = temp;
  /*
  temp->next = fetch_inst_free_list;
  fetch_inst_free_list = temp;
  */
}


/* not used any more */
void schedule_resume_ifetch(tick_t t) {}
void fetch_resume_ifetch(tick_t t, int arg) {}
void schedule_tlb_resume_ifetch(tick_t t, cache_access_packet* c, MSHR_STAMP_TYPE m) {}

