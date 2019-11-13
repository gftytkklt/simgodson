/*
 * eventq.h - event queue manager interfaces
 *
 * This file is part of the Alpha simulator tool suite written by
 * Raj Desikan as part of the Bullseye project.
 * It has been written by extending the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *  
 * 
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
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
 *
 *
 */

#ifndef EVENTQ_H
#define EVENTQ_H

#include <stdio.h>
#include <assert.h>
#include "host.h"
#include "misc.h"
#include "bitmap.h"

/* This module implements a time ordered event queue.  Users insert
 *
 */

/* event actions */
enum eventq_action {
  EventSetBit,			/* set a bit: int *, bit # */
  EventClearBit,		/* clear a bit: int *, bit # */
  EventSetFlag,			/* set a flag: int *, value */
  EventAddOp,			/* add a value to a summand */
  EventCallback,		/* call event handler: fn * */
  EventCallback2,		/* call event handler: fn * */
  EventCallback3,		/* call event handler: fn * */
  EventCallback4,       /* call event handler: fn * */
  EventCallback5,       /* call event handler: fn * */
  EventCallback7,		/* call event handler: fn * */
  EventCallback9,		/* call event handler: fn * */
  EventCallback10,		/* call event handler: fn * */
  EventCallback11,		/* call event handler: fn * */
  EventCallback12,		/* call event handler: fn * */
  EventCallbackPointer4 /* call event handler with a pointer parameter and 4 int */
};

/* ID zero (0) is unused */
typedef unsigned int EVENTQ_ID_TYPE;

/* event descriptor */
struct eventq_desc {
  struct eventq_desc *next;	/* next event in the sorted list */
  tick_t when;				/* time to schedule write back event */
  EVENTQ_ID_TYPE id;		/* unique event ID */
  enum eventq_action action;	/* action on event occurrance */
  union eventq_data {
    struct {
      BITMAP_ENT_TYPE *bmap;	/* bitmap to access */
      int sz;			/* bitmap size */
      int bitnum;		/* bit to set */
    } bit;
    struct {
      int *pflag;		/* flag to set */
      int value;
    } flag;
    struct {
      int *summand;		/* value to add to */
      int addend;		/* amount to add */
    } addop;
    struct {
      void (*fn)(tick_t time, int arg);	/* function to call */
      int arg;			/* argument to pass */
    } callback;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2);		/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
    } callback2;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3);	/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
    } callback3;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4);/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;         /* argument to pass */ 
    } callback4;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4,int arg5);/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;         /* argument to pass */ 
      int arg5;         /* argument to pass */ 
    } callback5;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4,
		 int arg5, int arg6, int arg7);	/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;			/* argument to pass */
      int arg5;			/* argument to pass */
      int arg6;			/* argument to pass */
      int arg7;			/* argument to pass */
    } callback7;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4,
		 int arg5, int arg6, int arg7,
		 int arg8, int arg9);	/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;			/* argument to pass */
      int arg5;			/* argument to pass */
      int arg6;			/* argument to pass */
      int arg7;			/* argument to pass */
      int arg8;			/* argument to pass */
      int arg9;			/* argument to pass */
    } callback9;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4,
		 int arg5, int arg6, int arg7,
		 int arg8, int arg9, int arg10);	/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;			/* argument to pass */
      int arg5;			/* argument to pass */
      int arg6;			/* argument to pass */
      int arg7;			/* argument to pass */
      int arg8;			/* argument to pass */
      int arg9;			/* argument to pass */
      int arg10;		/* argument to pass */
    } callback10;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4,
		 int arg5, int arg6, int arg7, int arg8,
		 int arg9, int arg10,int arg11);	/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;			/* argument to pass */
      int arg5;			/* argument to pass */
      int arg6;			/* argument to pass */
      int arg7;			/* argument to pass */
      int arg8;			/* argument to pass */
      int arg9;			/* argument to pass */
      int arg10;		/* argument to pass */
      int arg11;		/* argument to pass */
    } callback11;
    struct {
      void (*fn)(tick_t time,
		 int arg1, int arg2, int arg3, int arg4,
		 int arg5, int arg6, int arg7, int arg8, 
		 int arg9, int arg10,int arg11,int arg12);	/* function to call */
      int arg1;			/* argument to pass */
      int arg2;			/* argument to pass */
      int arg3;			/* argument to pass */
      int arg4;			/* argument to pass */
      int arg5;			/* argument to pass */
      int arg6;			/* argument to pass */
      int arg7;			/* argument to pass */
      int arg8;			/* argument to pass */
      int arg9;			/* argument to pass */
      int arg10;		/* argument to pass */
      int arg11;		/* argument to pass */
      int arg12;		/* argument to pass */
    } callback12;
	struct {
	  void (*fn)(tick_t time, void *data,
				 int arg1, int arg2, int arg3, int arg4);  /* function to call */
	  void *data;
	  int  arg1;
	  int  arg2;
	  int  arg3;
	  int  arg4;
	} callbackpointer4;
  } data;
};

#if 0
Conflicts...what do I set max_ecents to - HRISHI
/* initialize the event queue module, MAX_EVENT is the most events allowed
   pending, pass a zero if there is no limit */
void eventq_init(int max_events);
#endif
/* schedule an action that occurs at WHEN, action is visible at WHEN,
   and invisible before WHEN */
EVENTQ_ID_TYPE eventq_queue_setbit(tick_t when,
				   BITMAP_ENT_TYPE *bmap, int sz, int bitnum);
EVENTQ_ID_TYPE eventq_queue_clearbit(tick_t when, BITMAP_ENT_TYPE *bmap,
				     int sz, int bitnum);
EVENTQ_ID_TYPE eventq_queue_setflag(tick_t when,
				    int *pflag, int value);
EVENTQ_ID_TYPE eventq_queue_addop(tick_t when,
				  int *summand, int addend);
EVENTQ_ID_TYPE eventq_queue_callback(tick_t when,
				     void (*fn)(tick_t time, int arg),
				     int arg);

EVENTQ_ID_TYPE eventq_queue_callback2(tick_t when,
				     void (*fn)(tick_t time,
						int arg1, int arg2),
				     int arg1, int arg2);
EVENTQ_ID_TYPE eventq_queue_callback3(tick_t when,
				     void (*fn)(tick_t time, int arg1,
						int arg2, int arg3),
				     int arg1, int arg2, int arg3);
EVENTQ_ID_TYPE eventq_queue_callback4(tick_t when,
				     void (*fn)(tick_t time, int arg1,
						int arg2, int arg3, int arg4),
				     int arg1, int arg2, int arg3, int arg4);
EVENTQ_ID_TYPE eventq_queue_callback5(tick_t when,
				     void (*fn)(tick_t time, int arg1,
						int arg2, int arg3, int arg4,int arg5),
				     int arg1, int arg2, int arg3, int arg4,int arg5);
EVENTQ_ID_TYPE eventq_queue_callback7(tick_t when,
				     void (*fn)(tick_t time, int arg1, int arg2, int arg3,
						int arg4, int arg5, int arg6, int arg7),
				     int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7);
EVENTQ_ID_TYPE eventq_queue_callback9(tick_t when,
				     void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4,
						int arg5, int arg6, int arg7, int arg8, int arg9),
				     int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9);
EVENTQ_ID_TYPE eventq_queue_callback10(tick_t when,
				     void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4,
						int arg5, int arg6, int arg7, int arg8, int arg9,int arg10),
				     int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9,int arg10);
EVENTQ_ID_TYPE eventq_queue_callback11(tick_t when,
				     void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4,
						int arg5, int arg6, int arg7, int arg8, int arg9,int arg10, int arg11),
				     int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9,int arg10, int arg11);
EVENTQ_ID_TYPE eventq_queue_callback12(tick_t when,
				     void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4,
						int arg5, int arg6, int arg7, int arg8, int arg9,int arg10, int arg11, int arg12),
				     int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9,int arg10, int arg11, int arg12);
EVENTQ_ID_TYPE eventq_queue_callbackpointer4(tick_t when,
											 void (*fn)(tick_t time, void *data, int arg1, int arg2, int arg3, int arg4),
											 void *data, int arg1, int arg2, int arg3, int arg4);
/* execute an event immediately, returns non-zero if the event was
   located an deleted */
int eventq_execute(EVENTQ_ID_TYPE id);

/* remove an event from the eventq, action is never performed, returns
   non-zero if the event was located an deleted */
int eventq_remove(EVENTQ_ID_TYPE id);

/* service all events in order of occurrance until and at NOW */
void eventq_service_events(tick_t now);

void eventq_dump(FILE *stream);

void eventq_blow();
#endif /* EVENT_H */





