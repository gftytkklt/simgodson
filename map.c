/*
* map.c- map stage implementation
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
#include <assert.h>
#include "mips.h"                                                    
#include "regs.h"
#include "cache.h"
#include "bpred.h"
//#include "fetch.h"
//#include "decode.h"                                                     
//#include "map.h"
//#include "issue.h"
//#include "writeback.h"
//#include "commit.h"
//#include "lsq.h"
#include "eventq.h"
#include "resource.h"
#include "ptrace.h"

#include "istat.h"

#include "sim.h"
#include "godson2_cpu.h"

/* initialize the free RS_LINK pool */
static void
rslink_init(struct godson2_cpu *st,int nlinks) /* total number of RS_LINK available */
{
  int i;
  struct RS_link *link;

  st->rslink_free_list = NULL;
  for (i=0; i<nlinks; i++)
    {
      link = calloc(1, sizeof(struct RS_link));
      if (!link)
    	fatal("out of virtual memory");
      link->next = st->rslink_free_list;
      st->rslink_free_list = link;
    }
}

void 
map_stage_init(struct godson2_cpu *st)
{
  st->roq = (struct inst_descript **)
    calloc(roq_ifq_size, sizeof(struct inst_descript*));
  
  if (!st->roq) 
    fatal ("out of virtual memory");
  
  st->roq_num = 0;
  st-> roq_head = 0;
  st->roq_tail = 0;

  st->brq_num = 0;
  st->brq_head = 0;
  st->brq_tail = 0;
  st->brq = (struct inst_descript **)
    calloc(brq_ifq_size, sizeof(struct inst_descript*));
  
  if (!st->brq) 
    fatal ("out of virtual memory");

  /* used int rename register */
  st->int_rename_reg_num = 0;

  /* used fp rename register */
  st->fp_rename_reg_num = 0;

  rslink_init(st,MAX_RS_LINKS);
}

void 
map_stage(struct godson2_cpu *st)
{
  int roq_full,brq_full,lsq_full,intq_full,fpq_full;
  int grmt_full,frmt_full;
  int mapped = 0; /* mapped instruction in current cycle */

  /* check resources */
  roq_full  = (st->roq_num + map_width > roq_ifq_size);
  brq_full  = (st->brq_num >= brq_ifq_size - 1);
  lsq_full  = (st->lsq_num + map_width > lsq_ifq_size);
  intq_full = (st->int_issue_num + map_width > int_issue_ifq_size);
  fpq_full  = (st->fp_issue_num + map_width > fp_issue_ifq_size);
  grmt_full = st->int_rename_reg_num + map_width > int_rename_reg_size;
  frmt_full = st->fp_rename_reg_num + map_width > fp_rename_reg_size;

#if 0
  roq_count  += roq_num;
  roq_fcount += roq_full;
  brq_count  += brq_num;
  brq_fcount += brq_full;
  lsq_count  += lsq_num;
  lsq_fcount += lsq_full;
  intq_count += int_issue_num;
  intq_fcount+= intq_full;
  fpq_count  += fp_issue_num;
  fpq_fcount += fpq_full;
#endif
  
  if (roq_full || brq_full || lsq_full || intq_full || fpq_full || \
      grmt_full || frmt_full) {
    return;
  }

  /* process instructions */

  while (/* has instruction to map */
         st->decode_num!=0 && 
    	 /* has map bandwith left */
    	 mapped < map_width) {
    struct inst_descript *current;
    int in1,in2,in3,out1,out2;
    int op;
    int int_reg,fp_reg;

    current = st->decode_data[st->decode_head];

    op = current->op;
    in1 = current->in1;
    in2 = current->in2;
    in3 = current->in3;
    out1 = current->out1;
    out2 = current->out2;

    /* consume rename registers */
    int_reg = 0;
    fp_reg = 0;

    if (out1!=DNA) {
      if (out1<32 || out1==DHI){
    	int_reg++;
        st->fix_map_access++;
      }else{
    	fp_reg++;
        st->fp_map_access++;
      }
    }

    if (out2!=DNA) {
      if (out2<32 || out2==DLO)
    	int_reg++;
      else 
    	fp_reg++;
    }

    current->used_int_rename_reg = int_reg;
    current->used_fp_rename_reg = fp_reg;
    st->int_rename_reg_num += int_reg;
    st->fp_rename_reg_num += fp_reg;

    /* enter reorder queue */
    current->roqid = st->roq_tail;
    st->roq[st->roq_tail] = current;
    st->roq_tail = (st->roq_tail + 1) % roq_ifq_size;
    st->roq_num ++;

    /* if branch,enter brq */
    if (MD_OP_FLAGS(op) & F_CTRL) {
      if (st->pred->class==BPred2Level) {
        /* read ghr here */
    	if (!st->ghr_valid) {
    	  current->dir_update.recovery_shiftregs[0] = st->pred->dirpred.twolev->config.two.shiftregs[0];
    	}else{
    	  /*last cycle' decode update too */
    	  current->dir_update.recovery_shiftregs[0] = (st->pred->dirpred.twolev->config.two.shiftregs[0]<<1 | st->ghr_predict);
    	}
      }
      current->brqid = st->brq_tail;
      current->bdrdy = 0;
      st->brq[st->brq_tail] = current;
      st->brq_tail = (st->brq_tail + 1) % brq_ifq_size;
      st->brq_num ++;
    }else{
      /* not accurate for delay slots */
      current->brqid = st->brq_tail;
    }

    current->all_ready = OPERANDS_READY(current);
	/*
    if (current->all_ready) {
      readyq_enqueue(current);
    }
	*/

	/* allocate function unit */
	if (!current->trap && MD_OP_FUCLASS(op)!=0) {
	  current->fu = res_alloc(st,MD_OP_FUCLASS(op));
	}else {
	  current->fu = res_alloc(st,IntALU);
	  //myfprintf(stderr,"error code at %x\n",current->regs_PC);
	}

    current->in_intq = is_intq_op(op);

	if (!current->trap) {
	  /* enter issue queue */
	  issue_enqueue(st,current);

	  /* if mem op,enter load/store queue */
	  if (st->dcache && (MD_OP_FLAGS(op) & F_MEM)) {
	    lsq_enqueue(st,current);
            st->lsq_map_access++;
	  }else{
	    current->lsqid = st->lsq_tail;
	  }
	}

#ifdef ISTAT
    current->map_stamp = sim_cycle;
#endif

    st->decode_head = (st->decode_head + 1) % decode_ifq_size;
    st->decode_num --;

    mapped ++;

# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
    ptrace_newstage(current->ptrace_seq, PST_MAP, 0);
# endif
	
    current->mapped = TRUE;
  } 
  st->sim_map_insn += mapped;
}
