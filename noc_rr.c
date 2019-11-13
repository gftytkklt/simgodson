/*
* noc.c - network on chip stage implementation
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
#include "ptrace.h"
#include "cache.h"
#include "noc.h"
#include "istat.h"
#include "cache2mem.h"

void crossbar_switch(struct godson2_cpu *st)
{
  /* now nothing to do */
}

/* now router use simple X-Y routing algorithm, round robin arbitrating policy and credit-based flow control algoruthm */
void arbitrate(struct godson2_cpu *st)
{
  int type, i, j, req[NUM_DIRECTIONS+1];
  
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
	
    /* home inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    /* external write back should allow the internal write back issue first.*/
    if(st->router->egress_credit[type][HOME] > (type==REQ /*|| type == WTBK*/)?1:0 || type == RESP){
      for(i=0 ; i<NUM_DIRECTIONS ; i++){
		if(st->router->ingress_num[type][i] > 0)
       	  if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x == st->router->home_x &&
			 st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_y == st->router->home_y)
			  req[i] = 1;
	  }
	
	  for(i=1 ; i<=NUM_DIRECTIONS ; i++){
	    j = (st->router->egress_last_grant[type][HOME] + i) % NUM_DIRECTIONS;
	    if(req[j] == 1){  
			st->router->egress_grant[type][HOME] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
			st->router->egress_last_grant[type][HOME] = j;
		  break;
	    }
	  }
	}
  
    /* up inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][UP] > 0){
	  for(i=0 ; i<NUM_DIRECTIONS ; i++){
		if(st->router->ingress_num[type][i] > 0)
          if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x == st->router->home_x &&
			 st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_y < st->router->home_y)
			  req[i] = 1;
	  }
	
	  for(i=1 ; i<=NUM_DIRECTIONS ; i++){
	    j = (st->router->egress_last_grant[type][UP] + i) % NUM_DIRECTIONS;
	    if(req[j] == 1){  
		  st->router->egress_grant[type][UP] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
		  st->router->egress_last_grant[type][UP] = j; 
		  break;
	    }
	  }
	}
  
    /* down inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][DOWN] > 0){
	  for(i=0 ; i<NUM_DIRECTIONS ; i++){
		if (st->router->ingress_num[type][i] > 0)
          if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x == st->router->home_x &&
			 st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_y > st->router->home_y)
			  req[i] = 1;
	  }
	
	  for(i=1 ; i<=NUM_DIRECTIONS ; i++){
	    j = (st->router->egress_last_grant[type][DOWN] + i) % NUM_DIRECTIONS;
	    if(req[j] == 1){  
		  st->router->egress_grant[type][DOWN] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
		  st->router->egress_last_grant[type][DOWN] = j;
		  break;
	    }
	  }
    }
  
    /* left inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][LEFT] > 0){
	  for(i=0 ; i<NUM_DIRECTIONS ; i++){
		if(st->router->ingress_num[type][i] > 0)
          if(st->router->ingress[type][i][st->router->ingress_head[type][i]]->dest_x < st->router->home_x)
			req[i] = 1;
	  }
	
	  for(i=1 ; i<=NUM_DIRECTIONS ; i++){
	    j = (st->router->egress_last_grant[type][LEFT] + i) % NUM_DIRECTIONS;
	    if(req[j] == 1){  
		  st->router->egress_grant[type][LEFT] = st->router->ingress[type][j][st->router->ingress_head[type][j]];
		  st->router->egress_last_grant[type][LEFT]  = j;
		  break;
	    }
	  }
	}
  
    /* right inputbuffer arbitration */
    req[0] = req[1] = req[2] = req[3] = req[4] = req[5] = 0; /*req[5] is always 0*/
    if(st->router->egress_credit[type][RIGHT] > 0){
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
		  break;
	    }
	  }
    }
  }/* end of channel type loop */
}

