/*
* writeback.c - Execute + dcache access stage implementation
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
* INTERNET: raju@cs.utexas.edu
* US Mail:  8200B W. Gate Blvd, Austin, TX 78745
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "mips.h"                                                      
#include "regs.h"
#include "memory.h"
#include "syscall.h"   
#include "resource.h"
#include "sim.h" 
#include "godson2_cpu.h" 
#include "cache.h"
#include "tlb.h"
#include "bpred.h"
//#include "fetch.h" 
//#include "decode.h"
//#include "issue.h"
//#include "map.h"
//#include "writeback.h"
//#include "commit.h"
#include "loader.h"
#include "eventq.h"
#include "ptrace.h"

//#include "lsq.h"
#include "cache2mem.h"

#include "istat.h"

/*
 * the execution unit event queue implementation follows, the event queue
 * indicates which instruction will complete next, the writeback handler
 * drains this queue
 */

/* initialize the event queue structures */
static void
eventq_init(struct godson2_cpu *st)
{
  st->event_queue = NULL;
}

/* insert an event for RS into the event queue, event queue is sorted from
   earliest to latest event, event and associated side-effects will be
   apparent at the start of cycle WHEN */
void
eventq_queue_event(struct godson2_cpu *st, struct inst_descript *rs, tick_t when)
{
  struct RS_link *prev, *ev, *new_ev;

# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
    if (rs->completed)
      panic("event completed");
    if (when <= sim_cycle)
      panic("event occurred in the past");
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  /* get a free event record */
  RSLINK_NEW(new_ev, rs);
  new_ev->x.when = when;

  /* locate insertion point */
  for (prev=NULL, ev=st->event_queue;
       ev && ev->x.when < when;
       prev=ev, ev=ev->next);

  if (prev)
    {
      /* insert middle or end */
      new_ev->next = prev->next;
      prev->next = new_ev;
    }
  else
    {
      /* insert at beginning */
      new_ev->next = st->event_queue;
      st->event_queue = new_ev;
    }
}

/* return the next event that has already occurred, returns NULL when no
   remaining events or all remaining events are in the future */
static struct inst_descript *
eventq_next_event(struct godson2_cpu *st)
{
  struct RS_link *ev;

  if (st->event_queue && st->event_queue->x.when <= sim_cycle)
    {
      /* unlink and return first event on priority list */
      ev = st->event_queue;
      st->event_queue = st->event_queue->next;

      /* event still valid? */
      if (RSLINK_VALID(ev))
	    {
	      struct inst_descript *rs = RSLINK_RS(ev);

	      /* reclaim event record */
	      RSLINK_FREE(ev);

	      /* free none pipeline unit */
	      rs->fu->busy = 0;

	      /* event is valid, return resv station */
	      return rs;
	    }
      else
	    {
	      /* reclaim event record */
	      RSLINK_FREE(ev);

	      /* receiving inst was squashed, return next event */
	      return eventq_next_event(st);
	    }
    }
  else
    {
      /* no event or no event is ready */
      return NULL;
    }
}

void roq_dump(struct godson2_cpu *st)
{
  int index = st->roq_head;
  int num = st->roq_num;
  struct inst_descript *rs;

  myfprintf(stderr,"roq dump: head %d,tail %d,num %d\n",st->roq_head,st->roq_tail,st->roq_num);
  while (num>0) {
	rs = st->roq[index];
    myfprintf(stderr,"roq[%2d] pc=%08x(%s),rdy (%1d,%1d),q %1d,ni %1d,com %1d,fu %1d\n",index,rs->regs_PC,md_op2name[rs->op],rs->idep_ready[0],rs->idep_ready[1],rs->queued,rs->next_issue,rs->completed,rs->fu->fu_index);
    //myfprintf(stderr," brqid %2d,bdrdy %1d,brc %1d,in1 %2d,in2 %2d,out1 %2d,pt=%1d,br=%1d,spec=%2d,intq %1d\n",rs->brqid,rs->bdrdy,rs->brcompleted,rs->in1,rs->in2,rs->out1,rs->pred_taken,rs->br_taken,rs->spec_level,rs->in_intq);
    index = (index+1) % roq_ifq_size;
    num--;
  }
}


/* cancel upon a mispredicted branch instruction */
//void brerr_recover(tick_t now,int arg)
void brerr_recover(struct godson2_cpu *st, struct inst_descript *rs)
{
  //struct inst_descript *rs = (struct inst_descript *)arg;
  int roqid = rs->roqid;
  int i, roq_index = st->roq_tail;
  int roq_prev_tail = st->roq_tail;

  //myfprintf(stderr,"brerr at %x,recover to %x\n",rs->regs_PC,rs->recover_PC);
  
  /* roq */
  if (!rs->trap && !(MD_OP_FLAGS(rs->op) & F_CTRL_LIKELY)) {
      /* have to ensure its delay slot is in roq, see check_brq() */
	roqid = ( rs->roqid + 1 ) % roq_ifq_size;
  }

  //myfprintf(stderr,"roq h %d,t %d,n %d,id %d\n",roq_head,roq_tail,roq_num,roqid);

  //if (rs->regs_PC==0x400f96c) roq_dump();

  /* recover from the tail of the RUU towards the head until the branch index
     is reached, this direction ensures that the LSQ can be synchronized with
     the RUU */

  /* go to first element to squash */
  roq_index = (roq_index + (roq_ifq_size-1)) % roq_ifq_size;

  /* traverse to older insts until the (delay slot of) mispredicted branch is
   * encountered 
   */
  while (roq_index != roqid)
    {
      /* the roq should not drain since the mispredicted branch will remain */
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
      if (!st->roq_num)
	    panic("empty RUU");

        /* should meet up with the tail first */
      if (roq_index == st->roq_head) {
	    roq_dump(st);
	    panic("RUU head and tail broken");
	  }
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      /* recover any resources used by this roq operation */
      for (i=0; i<MAX_ODEPS; i++) {
	    RSLINK_FREE_LIST(st->roq[roq_index]->odep_list[i]);
	    /* blow away the consuming op list */
	    st->roq[roq_index]->odep_list[i] = NULL;
	  }

      /* squash this RUU entry */
      st->roq[roq_index]->tag++;

      /* free possible used non-pipeline unit */
      st->roq[roq_index]->fu->busy = 0;

      /* free brq item */
      if (MD_OP_FLAGS(st->roq[roq_index]->op) & F_CTRL) {
	    st->brq[st->roq[roq_index]->brqid] = NULL;
	    st->brq_tail = (st->brq_tail + brq_ifq_size - 1) % brq_ifq_size;
	    st->brq_num --;
      }

      /* free lsq item */
      if (st->dcache && (MD_OP_FLAGS(st->roq[roq_index]->op) & F_MEM)) {
    	lsq_cancel_one(st,st->roq[roq_index]->lsqid);
      }

      /* free rename registers */
      st->int_rename_reg_num -= st->roq[roq_index]->used_int_rename_reg;
      st->fp_rename_reg_num  -= st->roq[roq_index]->used_fp_rename_reg;

      /* free issue queue item */
      if (st->roq[roq_index]->queued) {
    	if (st->roq[roq_index]->in_intq) 
    	  st->int_issue_num --;
        else
          st->fp_issue_num --;
      }

# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
      /* indicate in pipetrace that this instruction was squashed */
      ptrace_endinst(st->roq[roq_index]->ptrace_seq);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	  
      /* free the instruction data */
      fetch_return_to_free_list(st,st->roq[roq_index]);

      st->roq[roq_index] = NULL;

      /* go to next earlier slot in the roq */
      roq_prev_tail = roq_index;
      roq_index = (roq_index + (roq_ifq_size-1)) % roq_ifq_size;
      st->roq_num--;
    }

  /* reset head/tail pointers to point to the mis-predicted branch */
  st->roq_tail = roq_prev_tail;

  tracer_recover(st,rs->spec_level,rs->recover_PC);

  bpred_recover(st->pred, rs->regs_PC, rs->stack_recover_idx, &rs->dir_update);

  st->ghr_valid = FALSE;

  //roq_dump();
}

void 
writeback_stage_init(struct godson2_cpu *st)
{
  eventq_init(st);
}


/* writeback completed operation results from the functional units to roq
   at this point, the output dependency chains of completing instructions
   are also walked to determine if any dependent instruction now has all
   of its register operands, if so the (nearly) ready instruction is inserted
   into the ready instruction queue */
void 
writeback_stage(struct godson2_cpu *st) 
{
  int i;
  struct inst_descript *rs;

  /* service all completed events */
  while ((rs = eventq_next_event(st)))
    {
      /* RS has completed execution and (possibly) produced a result */
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
      if (!OPERANDS_READY(rs) || rs->queued || !rs->issued || rs->completed)
	    panic("inst completed and !ready, !issued, or completed");
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      st->sim_writeback_insn ++;

      if (rs->regs_PC == 0x4122b8) {
		//dump_ready_queue(st);
		//printf("sim_cycle is 0x%x\n",sim_cycle);
		//exit(0);
	  }

      if (MD_OP_FLAGS(rs->op) & F_CTRL) {
	    /* for branches,more work to do. See BR_WTBK in godson simulator */
    	rs->brcompleted = TRUE;
      }else{
    	/* operation has completed */
    	rs->completed = TRUE;

        /* add access counters for our power model */
        if(MD_OP_FLAGS(rs->op) & F_ICOMP){
          st->fix_wb_access++;
        }else if(MD_OP_FLAGS(rs->op) & F_FCOMP){
          st->fp_wb_access++;
        }else if(MD_OP_FLAGS(rs->op) & F_MEM){
          st->fix_wb_access++;
          st->lsq_wb_access++;
        }
   
	if (MD_OP_FLAGS(rs->op) & F_MEM) {
    	  st->lsq[rs->lsqid].state = LSQ_WRITEBACK;
    	}
      }

#ifdef ISTAT
      rs->writeback_stamp = sim_cycle;
#endif

#if 0
      if (pred
	  && (MD_OP_FLAGS(rs->op) & F_CTRL)) {
	  bpred_access++;
	  bpred_update(pred,
		       /* branch address */rs->regs_PC,
		       /* actual target address */rs->regs_NPC,
		       /* taken? */rs->br_taken,
		       /* pred taken? */rs->pred_taken,
		       /* correct pred? */ !rs->mis_predict,
		       /* opcode */rs->op,
		       /* dir predictor update pointer */&rs->dir_update);
	}

      if ((MD_OP_FLAGS(rs->op) & F_CTRL) && rs->mis_predict) {
	  eventq_queue_callback(sim_cycle + 1, (void *) brerr_recover, (int) rs);
      }
#endif

# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
      /* entered writeback stage, indicate in pipe trace */
      ptrace_newstage(rs->ptrace_seq, PST_WRITEBACK,rs->mis_predict? PEV_MPDETECT : 0);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      /* broadcast results to consuming operations, this is more efficiently
	 accomplished by walking the output dependency chains of the
	 completed instruction */
      for (i=0; i<MAX_ODEPS; i++)
      {
    	if (rs->onames[i] != NA)
    	{
    	  struct RS_link *olink, *olink_next;

       /* update the speculative create vector, future operations
   	      get value from later creator or architected reg file */
    	  update_create_vector(st,rs);

	  /* walk output list, queue up ready operations */
    	  for (olink=rs->odep_list[i]; olink; olink=olink_next)
    	  {
    	    if (RSLINK_VALID(olink))
    	    {
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
    	      if (olink->rs->idep_ready[olink->x.opnum])
        		panic("%x output dependence already satisfied",olink->rs->regs_PC);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      	      /* input is now ready */
    	      olink->rs->idep_ready[olink->x.opnum] = TRUE;
    	      //myfprintf(stderr,"%x make %x(%d) ready\n",rs->regs_PC,olink->rs->regs_PC,olink->x.opnum);

    	    /* for convenience,we generate deplist at decode stage,
	         * if not mapped,skipped it
    	     */
            /* are all the register operands of target ready? */
    	      if (OPERANDS_READY(olink->rs) && olink->rs->mapped)
    	      {
    			rs->all_ready = 1;
      		/* yes! enqueue instruction as ready */
        		readyq_enqueue(st,olink->rs);
    	      }
    	    }

    	    /* grab link to next element prior to free */
    	    olink_next = olink->next;

    	    /* free dependence link element */
    	    RSLINK_FREE(olink);
    	  }
    	  /* blow away the consuming op list */
    	  rs->odep_list[i] = NULL;

    	} /* if not NA output */

      } /* for all outputs */

   } /* for all writeback events */
}


void check_brq(struct godson2_cpu *st)
{
  int i,brqid,roqid,roqid1;
  struct inst_descript *rs;

  /* check one more than item number to ensure return when empty */
  for (i=0,brqid=st->brq_head;i<=st->brq_num;i++){
    if (st->brq[brqid]==NULL) return;
    if (st->brq[brqid]->brcompleted)
      break;
    brqid = (brqid+1)%brq_ifq_size;
  }

  rs = st->brq[brqid];
  roqid = rs->roqid;
  roqid1 = (roqid+1) % roq_ifq_size;

  /* brbus is valid when:
   *   1. branch instruction has written back 
   *   2. no mis-prediction or its delayslot ready 
   * bdrdy is valid when:
   *   1. next roq item valid 
   *   2. next item not roq_head
   */
  if (rs->brcompleted && 
      (!rs->mis_predict || 
       (st->roq[roqid1] && roqid1!=st->roq_head))) {

      rs->brcompleted = FALSE;
      rs->completed = TRUE;

      if (st->pred) {
#if 0
	    bpred_update(pred,
		         /* branch address */rs->regs_PC,
		         /* actual target address */rs->regs_NPC,
		         /* taken? */rs->br_taken,
		         /* pred taken? */rs->pred_taken,
		         /* correct pred? */ !rs->mis_predict,
		         /* opcode */rs->op,
		         /* dir predictor update pointer */&rs->dir_update);
#else
	    bpred_update(st->pred,rs);
            st->bht_wb_access;
            st->btb_wb_access;
#endif
	  }

      st->sim_brbus_count ++;
      if (rs->bht_op) {
	    st->sim_bht_count++;
      }else if (rs->jr_op) {
	    st->sim_jr_count++;
      }

      //printf("brbus at %x,spec level=%x,mis=%x,seq=%x\n",rs->regs_PC,rs->spec_level,rs->mis_predict,rs->seq);

      if (rs->mis_predict) {
        st->sim_brerr_count ++;
	    if (rs->bht_op) {
	      st->sim_bhterr_count++;
	    }else if (rs->jr_op) {
	      st->sim_jrerr_count++;
	    }
	    //brerr_recover(sim_cycle,(int/*not safe in 64 bit?*/)rs);
	    brerr_recover(st,rs);
	    /* fetch should seen new pc next cycle */
	    st->fetch_istall_buf.stall |= BRANCH_STALL;
	/* now fetch stage will check this flag */
#if 0	
	/* Queue an event to restart fetch */
	eventq_queue_callback(sim_cycle + 1, 
	    (void *) fetch_resume_ifetch,
	    (int) BRANCH_STALL);
#endif
      }
  }
}
