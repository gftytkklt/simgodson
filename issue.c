/*
* issue.c - issue stage implementation
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
#include "resource.h"
#include "eventq.h"
#include "cache.h"
#include "bpred.h"
//#include "fetch.h"
//#include "map.h"
//#include "issue.h"
//#include "writeback.h"
#include "sim.h"
#include "godson2_cpu.h"
#include "ptrace.h"

#include "istat.h"

/* for each function unit,we maintain a ready queue and a next issue item*/
static int num_fu = 0;

/* initialize the ready queue structures for each function unit */
static void
readyq_init (struct godson2_cpu *st)
{
  int i;
  char unitname[80];

  num_fu = res_get_fu_number(st);

  st->ready_queue =
    (struct RS_link **) calloc (num_fu, sizeof (struct RS_link *));
  st->next_issue_item =
    (struct RS_link **) calloc (num_fu, sizeof (struct RS_link *));

  st->fu_inst_num =
    (counter_t *) (counter_t *) calloc (num_fu, sizeof (counter_t));

  if (!st->ready_queue || !st->next_issue_item || !st->fu_inst_num)
    fatal ("out of virtual memory");

  for (i = 0; i < num_fu; i++)
    {
      st->fu_inst_num[i] = 0;
      sprintf (unitname, "unit %d", i);
      stat_reg_counter (st->sdb, unitname,
			"instructions for this function unit",
			&st->fu_inst_num[i], st->fu_inst_num[i], NULL);
    }
}

/* insert ready node into the ready list of its function unit.
 */
void
readyq_enqueue (struct godson2_cpu *st,struct inst_descript *rs)	/* RS to enqueue */
{
  struct RS_link *prev, *node, *new_node;
  int fu;

  fu = rs->fu->fu_index;

  rs->queued = TRUE;

  /* get a free ready list node */
  RSLINK_NEW (new_node, rs);
  new_node->x.seq = rs->seq;

  /* insert queue in program order (earliest seq first) */
  for (prev = NULL, node = st->ready_queue[fu];
       node && (node->x.seq < rs->seq); prev = node, node = node->next);

  if (prev)
    {
      /* insert middle or end */
      new_node->next = prev->next;
      prev->next = new_node;
    }
  else
    {
      /* insert at beginning */
      new_node->next = st->ready_queue[fu];
      st->ready_queue[fu] = new_node;
    }
}

void
issue_stage_init (struct godson2_cpu *st)
{
  st->int_issue = (struct issue_queue *)
    calloc (int_issue_ifq_size, sizeof (struct issue_queue));

  st->fp_issue = (struct issue_queue *)
    calloc (fp_issue_ifq_size, sizeof (struct issue_queue));

  if (!st->int_issue || !st->fp_issue)
    fatal ("out of virtual memory");

  st->int_issue_num = 0;
  st->fp_issue_num = 0;

#if 0
  int_slots = (int *) calloc (map_width, sizeof (int), 1);
  fp_slots = (int *) calloc (map_width, sizeof (int), 1);
  if (!int_slots || !fp_slots)
    fatal ("out of virtual memory");
#endif

  readyq_init (st);
}

#if 0
int
get_int_issue_slot (void)
{
  int i, j;
  if (int_slot_num == 0)
    {
      /* find out which slot to contain new instructions,current godson-2 
       * search from both ends,find two slot for each direction 
       */
      for (i = 0, j = 0; (i < int_issue_ifq_size) && (j < (map_width / 2));
	   i++)
	{
	  if (int_issue[i].fu == FUClass_NA)
	    {
	      int_slots[j] = i;
	      j++;
	    }
	}
      for (i = int_issue_ifq_size - 1, j = map_width - 1;
	   (i >= 0) && (j >= (map_width / 2)); i--)
	{
	  if (int_issue[i].fu == FUClass_NA)
	    {
	      int_slots[j] = i;
	      j--;
	    }
	}
    }
  return int_slots[int_slot_num++];
}

int
get_fp_issue_slot (void)
{
  int i, j;
  if (fp_slot_num == 0)
    {
      /* find out which slot to contain new instructions,current godson-2 
       * search from both ends,find two slot for each direction 
       */
      for (i = 0, j = 0; (i < fp_issue_ifq_size) && (j < (map_width / 2));
	   i++)
	{
	  if (fp_issue[i].fu == FUClass_NA)
	    {
	      fp_slots[j] = i;
	      j++;
	    }
	}
      for (i = fp_issue_ifq_size - 1, j = map_width - 1;
	   (i >= 0) && (j >= (map_width / 2)); i--)
	{
	  if (fp_issue[i].fu == FUClass_NA)
	    {
	      fp_slots[j] = i;
	      j--;
	    }
	}
    }
  return fp_slots[fp_slot_num++];
}
#endif

void
issue_enqueue (struct godson2_cpu *st,struct inst_descript *rs)
{
  rs->queued = TRUE;

#if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  /* entered issue stage, indicate in pipe trace */
  ptrace_newstage (rs->ptrace_seq, PST_ISSUE, 0);
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  
  if (rs->in_intq)
    st->int_issue_num++;
  else
    st->fp_issue_num++;

  if (rs->all_ready)
    readyq_enqueue(st,rs);
  /* use readyq, no need for a physical issue queue */
}

/* follow the ready queue link,return next valid node,free squashed nodes
 * met
 */
static struct RS_link *
next_valid_node (struct godson2_cpu *st, struct RS_link *prev, struct RS_link *node, int fu_index)
{
  struct RS_link *next;

  while (node && !RSLINK_VALID (node))
    {
      next = node->next;
      RSLINK_FREE (node);
      node = next;
    }

  if (prev)
    {
      prev->next = node;
    }
  else
    {
      st->ready_queue[fu_index] = node;
    }

  return node;
}

/* delete node from ready queue q */
static void
delete_node (struct godson2_cpu *st, struct RS_link *n, int q)
{
  struct RS_link *prev, *node;

  /* lookup n in queue[q] */
  for (prev = NULL, node = st->ready_queue[q];
       node && (node != n); prev = node, node = node->next);

  /* must in the queue */
  assert (node == n);

  if (prev)
    {
      prev->next = n->next;
    }
  else
    {
      st->ready_queue[q] = n->next;
    }

  RSLINK_FREE (n);
}

void
dump_ready_queue (struct godson2_cpu *st)
{
  int i;
  struct RS_link *prev = NULL, *node;
  for (i = 0; i < num_fu; i++)
    {
      node = next_valid_node (st, NULL, st->ready_queue[i], i);
      if (node)
    	myfprintf (stderr, "ready queue %d:\n", i);
      while (node)
	{
	  myfprintf (stderr, "pc=%x,seq=%d\n", node->rs->regs_PC,
		     node->rs->seq);
	  prev = node;
	  node = next_valid_node (st, prev, node->next, i);
	}
    }
}

/* attempt to issue all operations in the ready queue; insts in the ready
   instruction queue have all register dependencies satisfied
 */
void
issue_stage (struct godson2_cpu *st)
{
  int issued = 0;
  struct RS_link *node;
  int fu_index;
  struct res_template *fu;
  struct inst_descript *rs;

  /* we maintain a ready queue for each function unit, every cycle,
   * we try to:
   *   1. issue the marked ready instruction for each unit
   *   2. mark a ready instruction as candidate for issue next cycle
   * ready queue order is maintained at insert time. 
   */
  //dump_ready_queue();

  for (fu_index = 0; fu_index < num_fu; fu_index++)
    {

      if (!sim_has_bypass) {
	    /* issue the chosen inst */
    	node = st->next_issue_item[fu_index];
    	/* has been cancelled */
    	if (node && !RSLINK_VALID (node))
    	{
    	  node = st->next_issue_item[fu_index] = NULL;
    	}
      }else{
    	/* get the first valid entry */
    	node = next_valid_node (st, NULL, st->ready_queue[fu_index], fu_index);
      }

      /* has valid chosen inst to issue,issue it */
      if (node)
	{
	  rs = node->rs;
	  fu = rs->fu;

# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
	  /* issue operation, both reg and mem deps have been satisfied */
	  if (!OPERANDS_READY (rs) || !rs->queued
	      || rs->issued || rs->completed)
	    panic ("issued inst !ready, issued, or completed");

# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

	  /* if it has been chosen and the function unit is free,issue it */
	  //assert (rs->next_issue);

	  if (!fu->master->busy && !fu->busy)
	    {

	      assert (fu->fu_index == fu_index);

	      /* got one! issue inst to functional unit */
	      rs->issued = TRUE;

	      rs->queued = FALSE;

#ifdef ISTAT
	      rs->issue_stamp = sim_cycle;
#endif

	      if (fu->class == IntDIV) {
    		fu->issuelat = 1; //rs->div_delay;
    		fu->busy = 1;
    		fu->oplat = rs->div_delay + 1;
	      }

              /* access counters added here for our power model */
              switch (fu->class) {
                case IntALU:
                  st->fix_issue_access++;
                  st->fix_add_access++;
                  break;
                case IntBR:
                  st->fix_issue_access++;
                  st->fix_br_access++;
                  break;
                case RdPort:
                  st->fix_issue_access++;
                  st->fix_mem_access++;
                  break;
                case WrPort:
                  st->fix_issue_access++;
                  st->fix_mem_access++;
                  break;
                case IntMULT:
                  st->fix_issue_access++;
                  st->fix_mult_access++;
                  break;
                case IntDIV:
                  st->fix_issue_access++;
                  st->fix_div_access++;
                  break;
                case FloatADD:
                  st->fp_issue_access++;
                  st->fp_add_access++;
                  break;
                case FpBR:
                  st->fp_issue_access++;
                  st->fp_br_access++;
                  break;
                case FloatCMP:
                  st->fp_issue_access++;
                  st->fp_cmp_access++;
                  break;
                case FloatCVT:
                  st->fp_issue_access++;
                  st->fp_cvt_access++;
                  break;
                case FloatMULT:
                  st->fp_issue_access++;
                  st->fp_mult_access++;
                  break;
                case FloatDIV:
                  st->fp_issue_access++;
                  st->fp_div_access++;
                  break;
              }

	      /* schedule functional unit release event */
	      fu->master->busy = fu->issuelat;

	      if (!st->dcache) {
	    	/* use deterministic functional unit latency */
    		eventq_queue_event (st, rs, sim_cycle + fu->oplat);
	      }else{
    		if (!(MD_OP_FLAGS(rs->op) & F_MEM)) {
    		  /* use deterministic functional unit latency */
    		  eventq_queue_event (st, rs, sim_cycle + fu->oplat);

    		}else{
	    	  /* currently we implement detailed memory pipeline */
    		  lsq_issue(st, rs);

    		}
	      }

	      /* one more inst issued */
	      issued++;

	      /* one more inst for fu */
	      st->fu_inst_num[fu_index]++;

#if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	      /* entered execute stage, indicate in pipe trace */
	      ptrace_newstage(rs->ptrace_seq, PST_EXECUTE, 0);
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

	      //myfprintf(stderr,"issued %x\n",rs->seq);

	      if (rs->in_intq)
    		st->int_issue_num--;
	      else
    		st->fp_issue_num--;

	      delete_node(st, node, fu_index);
	    }

	  /* unchosen */
	  rs->next_issue = FALSE;
	}

      if (!sim_has_bypass) {
    	/* rechoose next issue inst */

    	/* get the first valid entry */
    	node = st->next_issue_item[fu_index] = next_valid_node (st, NULL, st->ready_queue[fu_index], fu_index);
    	if (!node)
    	  continue;

    	rs = node->rs;
    	rs->next_issue = TRUE;
      }
    }

  /* total issued instruction */
  st->sim_issue_insn += issued;

}
