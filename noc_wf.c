/*
 * noc_wf.c - implementing NOC with Wave Front Allocator
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

void crossbar_switch(struct godson2_cpu *st)
{
  /* now nothing to do */
}

static int select_dest_channel(struct inputbuffer* entry, struct godson2_cpu *st) 
{
  int x = st->router->home_x;
  int y = st->router->home_y;
  if (entry->dest_x == x) {
	if (entry->dest_y == y)
	  return HOME;
	else if (entry->dest_y > y)
	  return DOWN;
	else /* entry.dest_y < y */
	  return UP;
  }
  else if (entry->dest_x < x) 
	return LEFT;
  else /* entry.dest_x > x */ 
	return RIGHT;
}

/* Wave Front allocation scheme */
#define WF_SEARCH_DEPTH 8
void arbitrate(struct godson2_cpu *st)
{
  struct {
	int request;
	struct inputbuffer *req_entry;
  } reqs[NUM_TYPES][NUM_DIRECTIONS][NUM_DIRECTIONS];
  
  char row_token[NUM_TYPES][NUM_DIRECTIONS];
  char col_token[NUM_TYPES][NUM_DIRECTIONS];

  memset(reqs, 0, sizeof(reqs));
  memset(row_token, 0, sizeof(row_token));
  memset(col_token, 0, sizeof(col_token));
  
  int type, i, j, k, head;
  struct inputbuffer** in;
	
  for(type = 0; type < NUM_TYPES ; type++) {
    for(i = 0 ; i < NUM_DIRECTIONS ; i++){
      st->router->egress_grant[type][i] = NULL;
    }
  }

  assert(WF_SEARCH_DEPTH <= router_ifq_size);
  
  for (type = 0; type < NUM_TYPES; type++) {
	for (i = 0; i < NUM_DIRECTIONS; i++) {
	  if (st->router->ingress_num[type][i] == 0)
		continue;
	  in = st->router->ingress[type][i];
	  head = st->router->ingress_head[type][i];
	  j = k = 0;
	  while(j < WF_SEARCH_DEPTH && k < router_ifq_size) {
		int pos = (head + k) % router_ifq_size;
		if (in[pos]) {
		  int dest_channel = select_dest_channel(in[pos], st);
		  if (reqs[type][i][dest_channel].request == 0) {
			reqs[type][i][dest_channel].request = 1;
			reqs[type][i][dest_channel].req_entry = in[pos];
			j++;
		  }
		}
		k++;
	  }
	}
  }

  for (type = 0; type < NUM_TYPES; type++) {
	int pri = st->router->priority[type];
	int x, y;
	for (i = 0; i < NUM_DIRECTIONS; i++) {
	  for (x = 0; x < NUM_DIRECTIONS; x++) {
		if (st->router->ingress_num[type][x] == 0)
		  continue;
		y = (pri + NUM_DIRECTIONS - x) % NUM_DIRECTIONS;
		if (reqs[type][x][y].request && !row_token[type][x] && !col_token[type][y] &&
			st->router->egress_credit[type][y] > ((type == REQ && y == HOME) ? 1 : 0)) {
		  st->router->egress_grant[type][y] = reqs[type][x][y].req_entry;
		  row_token[type][x] = 1;
		  col_token[type][y] = 1;
		  st->router->priority[type] = (pri + 1) % NUM_DIRECTIONS;
		}
	  }
	  pri = (pri + 1) % NUM_DIRECTIONS;
	}
  }
}
