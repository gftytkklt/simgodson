/*
 * eventq.c - event queue manager routines
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
 */

#include <stdio.h>
#include <stdlib.h>

#include "host.h"
#include "misc.h"
#include "mips.h"
#include "eventq.h"
#include "issue.h"
#include "writeback.h"



int eventq_max_events;
int eventq_event_count;
struct eventq_desc *eventq_pending;
struct eventq_desc *eventq_free;

extern void cpu_execute(tick_t now, struct godson2_cpu *st);
extern tick_t sim_cycle;

static EVENTQ_ID_TYPE next_ID = 1;
#if 0
conflicts - what do I set max_events to - Hrishi
static void  eventq_init(int max_events)
  {
	eventq_max_events = max_events;
	eventq_event_count = 0;
	eventq_pending = NULL;
	eventq_free = NULL;
  }
#endif

#ifdef SYNC_DVFS

#define __QUEUE_EVENT(WHEN, ID, ACTION)									\
  struct eventq_desc *prev, *ev, *new;									\
  /* get a free event descriptor */										\
  if (!eventq_free)														\
    {																	\
      if (eventq_max_events && eventq_event_count >= eventq_max_events)	\
		panic("too many events");										\
      eventq_free = calloc(1, sizeof(struct eventq_desc));				\
    }																	\
  new = eventq_free;													\
  eventq_free = eventq_free->next;										\
  /* plug in event data */												\
  new->when = (WHEN); (ID) = new->id = next_ID++; ACTION;				\
  /* locate insertion point */											\
  for (prev=NULL,ev=eventq_pending;										\
       ev && ev->when < when;											\
       prev=ev, ev=ev->next);											\
  /* insert new record */												\
  if (prev)																\
    {																	\
      /* insert middle or end */										\
      new->next = prev->next;											\
      prev->next = new;													\
    }																	\
  else																	\
    {																	\
      /* insert beginning */											\
      new->next = eventq_pending;										\
      eventq_pending = new;												\
    }

#else

#define __QUEUE_EVENT(WHEN, ID, ACTION)									\
  struct eventq_desc *prev, *ev, *new;									\
  /* get a free event descriptor */										\
  if (!eventq_free)														\
    {																	\
      if (eventq_max_events && eventq_event_count >= eventq_max_events)	\
		panic("too many events");										\
      eventq_free = calloc(1, sizeof(struct eventq_desc));				\
    }																	\
  new = eventq_free;													\
  eventq_free = eventq_free->next;										\
  /* plug in event data */												\
  new->when = (WHEN); (ID) = new->id = next_ID++; ACTION;				\
  /* locate insertion point */											\
   if((new->action==EventCallback)&&new->data.callback.fn==(void*)cpu_execute)  /*cpu_execute event*/  \
   for (prev=NULL,ev=eventq_pending;					\
        ev && ev->when <= (WHEN);	\
        prev=ev, ev=ev->next);						\
   else                                              \
   for (prev=NULL,ev=eventq_pending;										\
       ev && ev->when < when;											\
       prev=ev, ev=ev->next);											\
  /* insert new record */												\
  if (prev)																\
    {																	\
      /* insert middle or end */										\
      new->next = prev->next;											\
      prev->next = new;													\
    }																	\
  else																	\
    {																	\
      /* insert beginning */											\
      new->next = eventq_pending;										\
      eventq_pending = new;												\
    }

#endif

EVENTQ_ID_TYPE
eventq_queue_setbit(tick_t when,
					BITMAP_ENT_TYPE *bmap, int sz, int bitnum)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventSetBit; new->data.bit.bmap = bmap;	\
				new->data.bit.sz = sz; new->data.bit.bitnum = bitnum);
  return id;
}

EVENTQ_ID_TYPE
eventq_queue_clearbit(tick_t when,
					  BITMAP_ENT_TYPE *bmap, int sz, int bitnum)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventClearBit; new->data.bit.bmap = bmap;	\
				new->data.bit.sz = sz; new->data.bit.bitnum = bitnum);
  return id;
}

EVENTQ_ID_TYPE
eventq_queue_setflag(tick_t when, int *pflag, int value)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventSetFlag;				\
				new->data.flag.pflag = pflag; new->data.flag.value = value);
  return id;
}

EVENTQ_ID_TYPE
eventq_queue_addop(tick_t when, int *summand, int addend)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventAddOp;				\
				new->data.addop.summand = summand;			\
				new->data.addop.addend = addend);
  return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback(tick_t when,
					  void (*fn)(tick_t time, int arg), int arg)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback; new->data.callback.fn = fn;\
				new->data.callback.arg = arg);
  return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback2(tick_t when,
					   void (*fn)(tick_t now, int arg1, int arg2),
					   int arg1, int arg2)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback2; new->data.callback2.fn = fn;\
				new->data.callback2.arg1 = arg1;			\
				new->data.callback2.arg2 = arg2);
  return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback3(tick_t when,
					   void (*fn)(tick_t time, int arg1, int arg2, int arg3),
					   int arg1, int arg2, int arg3)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback3; new->data.callback3.fn = fn;\
				new->data.callback3.arg1 = arg1;			\
				new->data.callback3.arg2 = arg2;                        \
				new->data.callback3.arg3 = arg3;)
	return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback4(tick_t when,
					   void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4),
					   int arg1, int arg2, int arg3, int arg4)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback4; new->data.callback4.fn = fn;\
				new->data.callback4.arg1 = arg1;			\
				new->data.callback4.arg2 = arg2;                        \
				new->data.callback4.arg3 = arg3;                        \
				new->data.callback4.arg4 = arg4;)
	return id;
}
EVENTQ_ID_TYPE
eventq_queue_callback5(tick_t when,
					   void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4,int arg5),
					   int arg1, int arg2, int arg3, int arg4,int arg5)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback5; new->data.callback5.fn = fn;\
				new->data.callback5.arg1 = arg1;			\
				new->data.callback5.arg2 = arg2;                        \
				new->data.callback5.arg3 = arg3;                        \
				new->data.callback5.arg4 = arg4;						\
				new->data.callback5.arg5 = arg5;)
	return id;
}


EVENTQ_ID_TYPE
eventq_queue_callback7(tick_t when,
					   void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7),
					   int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback7; new->data.callback7.fn = fn;\
				new->data.callback7.arg1 = arg1;		\
				new->data.callback7.arg2 = arg2;        \
				new->data.callback7.arg3 = arg3;        \
				new->data.callback7.arg4 = arg4;		\
				new->data.callback7.arg5 = arg5;		\
				new->data.callback7.arg6 = arg6;		\
				new->data.callback7.arg7 = arg7;)
	return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback9(tick_t when,
					   void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9),
					   int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback9; new->data.callback9.fn = fn;\
				new->data.callback9.arg1 = arg1;		\
				new->data.callback9.arg2 = arg2;        \
				new->data.callback9.arg3 = arg3;        \
				new->data.callback9.arg4 = arg4;		\
				new->data.callback9.arg5 = arg5;		\
				new->data.callback9.arg6 = arg6;		\
				new->data.callback9.arg7 = arg7;		\
				new->data.callback9.arg8 = arg8;		\
				new->data.callback9.arg9 = arg9;)
	return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback10(tick_t when,
						void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10),
						int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback10; new->data.callback10.fn = fn;\
				new->data.callback10.arg1 = arg1;		\
				new->data.callback10.arg2 = arg2;       \
				new->data.callback10.arg3 = arg3;       \
				new->data.callback10.arg4 = arg4;		\
				new->data.callback10.arg5 = arg5;		\
				new->data.callback10.arg6 = arg6;		\
				new->data.callback10.arg7 = arg7;		\
				new->data.callback10.arg8 = arg8;		\
				new->data.callback10.arg9 = arg9;		\
				new->data.callback10.arg10 = arg10;)
	return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback11(tick_t when,
						void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10,int arg11),
						int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10,int arg11)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback11; new->data.callback11.fn = fn;\
				new->data.callback11.arg1 = arg1;		\
				new->data.callback11.arg2 = arg2;       \
				new->data.callback11.arg3 = arg3;       \
				new->data.callback11.arg4 = arg4;		\
				new->data.callback11.arg5 = arg5;		\
				new->data.callback11.arg6 = arg6;		\
				new->data.callback11.arg7 = arg7;		\
				new->data.callback11.arg8 = arg8;		\
				new->data.callback11.arg9 = arg9;		\
				new->data.callback11.arg10 = arg10;		\
				new->data.callback11.arg11 = arg11;)
	return id;
}

EVENTQ_ID_TYPE
eventq_queue_callback12(tick_t when,
						void (*fn)(tick_t time, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11, int arg12),
						int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11, int arg12)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallback12; new->data.callback12.fn = fn;\
				new->data.callback12.arg1 = arg1;		\
				new->data.callback12.arg2 = arg2;       \
				new->data.callback12.arg3 = arg3;       \
				new->data.callback12.arg4 = arg4;		\
				new->data.callback12.arg5 = arg5;		\
				new->data.callback12.arg6 = arg6;		\
				new->data.callback12.arg7 = arg7;		\
				new->data.callback12.arg8 = arg8;		\
				new->data.callback12.arg9 = arg9;		\
				new->data.callback12.arg10 = arg10;		\
				new->data.callback12.arg11 = arg11;		\
				new->data.callback12.arg12 = arg12;)
	return id;
}

EVENTQ_ID_TYPE
eventq_queue_callbackpointer4(tick_t when,
							  void (*fn)(tick_t time, void *data, int arg1, int arg2, int arg3, int arg4),
							  void *data, int arg1, int arg2, int arg3, int arg4)
{
  EVENTQ_ID_TYPE id;
  __QUEUE_EVENT(when, id,						\
				new->action = EventCallbackPointer4; new->data.callbackpointer4.fn = fn;\
				new->data.callbackpointer4.data = data;		\
				new->data.callbackpointer4.arg1 = arg1;		\
				new->data.callbackpointer4.arg2 = arg2;		\
				new->data.callbackpointer4.arg3 = arg3;		\
				new->data.callbackpointer4.arg4 = arg4;)		
	return id;
}

#define EXECUTE_ACTION(ev, now)											\
  /* execute action */													\
  switch (ev->action) {													\
  case EventSetBit:														\
    BITMAP_SET(ev->data.bit.bmap, ev->data.bit.sz, ev->data.bit.bitnum); \
    break;																\
  case EventClearBit:													\
    BITMAP_CLEAR(ev->data.bit.bmap, ev->data.bit.sz, ev->data.bit.bitnum); \
    break;																\
  case EventSetFlag:													\
    *ev->data.flag.pflag = ev->data.flag.value;							\
    break;																\
  case EventAddOp:														\
    *ev->data.addop.summand += ev->data.addop.addend;					\
    break;																\
  case EventCallback:													\
    (*ev->data.callback.fn)(now, ev->data.callback.arg);				\
    break;																\
  case EventCallback2:													\
	assert(*ev->data.callback2.fn);										\
    (*ev->data.callback2.fn)(now,                                       \
							 ev->data.callback2.arg1,					\
							 ev->data.callback2.arg2);					\
    break;																\
  case EventCallback3:                                                  \
	assert (*ev->data.callback3.fn);									\
	(*ev->data.callback3.fn)(now,										\
							 ev->data.callback3.arg1,					\
							 ev->data.callback3.arg2,					\
							 ev->data.callback3.arg3);					\
	break;																\
  case EventCallback4:                                                  \
	assert (*ev->data.callback4.fn);									\
	(*ev->data.callback4.fn)(now,										\
							 ev->data.callback4.arg1,					\
							 ev->data.callback4.arg2,					\
							 ev->data.callback4.arg3,					\
							 ev->data.callback4.arg4);					\
	break;																\
  case EventCallback5:                                                  \
	assert (*ev->data.callback5.fn);									\
	(*ev->data.callback5.fn)(now,										\
							 ev->data.callback5.arg1,					\
							 ev->data.callback5.arg2,					\
							 ev->data.callback5.arg3,					\
							 ev->data.callback5.arg4,					\
							 ev->data.callback5.arg5);					\
	break;																\
  case EventCallback7:													\
	assert(*ev->data.callback7.fn);										\
    (*ev->data.callback7.fn)(now,										\
							 ev->data.callback7.arg1,					\
							 ev->data.callback7.arg2,					\
							 ev->data.callback7.arg3,					\
							 ev->data.callback7.arg4,					\
							 ev->data.callback7.arg5,					\
							 ev->data.callback7.arg6,					\
							 ev->data.callback7.arg7);					\
    break;																\
  case EventCallback9:													\
	assert(*ev->data.callback9.fn);										\
    (*ev->data.callback9.fn)(now,										\
							 ev->data.callback9.arg1,					\
							 ev->data.callback9.arg2,					\
							 ev->data.callback9.arg3,					\
							 ev->data.callback9.arg4,					\
							 ev->data.callback9.arg5,					\
							 ev->data.callback9.arg6,					\
							 ev->data.callback9.arg7,					\
							 ev->data.callback9.arg8,					\
							 ev->data.callback9.arg9);					\
	break;																\
  case EventCallback10:													\
	assert(*ev->data.callback10.fn);									\
    (*ev->data.callback10.fn)(now,										\
							  ev->data.callback10.arg1,					\
							  ev->data.callback10.arg2,					\
							  ev->data.callback10.arg3,					\
							  ev->data.callback10.arg4,					\
							  ev->data.callback10.arg5,					\
							  ev->data.callback10.arg6,					\
							  ev->data.callback10.arg7,					\
							  ev->data.callback10.arg8,					\
							  ev->data.callback10.arg9,					\
							  ev->data.callback10.arg10);				\
    break;																\
  case EventCallback11:													\
	assert(*ev->data.callback11.fn);									\
    (*ev->data.callback11.fn)(now,										\
							  ev->data.callback11.arg1,					\
							  ev->data.callback11.arg2,					\
							  ev->data.callback11.arg3,					\
							  ev->data.callback11.arg4,					\
							  ev->data.callback11.arg5,					\
							  ev->data.callback11.arg6,					\
							  ev->data.callback11.arg7,					\
							  ev->data.callback11.arg8,					\
							  ev->data.callback11.arg9,					\
							  ev->data.callback11.arg10,				\
							  ev->data.callback11.arg11);				\
    break;																\
  case EventCallback12:													\
	assert(*ev->data.callback12.fn);									\
    (*ev->data.callback12.fn)(now,										\
							  ev->data.callback12.arg1,					\
							  ev->data.callback12.arg2,					\
							  ev->data.callback12.arg3,					\
							  ev->data.callback12.arg4,					\
							  ev->data.callback12.arg5,					\
							  ev->data.callback12.arg6,					\
							  ev->data.callback12.arg7,					\
							  ev->data.callback12.arg8,					\
							  ev->data.callback12.arg9,					\
							  ev->data.callback12.arg10,				\
							  ev->data.callback12.arg11,				\
							  ev->data.callback12.arg12);				\
    break;																\
  case EventCallbackPointer4:											\
	assert(*ev->data.callbackpointer4.fn);								\
    (*ev->data.callbackpointer4.fn)(now,								\
									ev->data.callbackpointer4.data,		\
									ev->data.callbackpointer4.arg1,		\
									ev->data.callbackpointer4.arg2,		\
									ev->data.callbackpointer4.arg3,		\
									ev->data.callbackpointer4.arg4);	\
	break;                                                              \
 default:										                        \
panic("bogus event action");					                        \
}

/* execute an event immediately, returns non-zero if the event was
   located an deleted */
int
eventq_execute(EVENTQ_ID_TYPE id)
{
struct eventq_desc *prev, *ev;

 for (prev=NULL,ev=eventq_pending; ev; prev=ev,ev=ev->next)
   {
	 if (ev->id == id)
	   {
		 if (prev)
		   {
			 /* middle of end of list */
			 prev->next = ev->next;
		   }
		 else /* !prev */
		   {
			 /* beginning of list */
			 eventq_pending = ev->next;
		   }

		 /* handle action, now is munged */
		 EXECUTE_ACTION(ev, 0);

		 /* put event on free list */
		 ev->next = eventq_free;
		 eventq_free = ev;

		 /* return success */
		 return TRUE;
	   }
   }
 /* not found */
 return FALSE;
}

/* remove an event from the eventq, action is never performed, returns
   non-zero if the event was located an deleted */
int
eventq_remove(EVENTQ_ID_TYPE id)
{
  struct eventq_desc *prev, *ev;

  for (prev=NULL,ev=eventq_pending; ev; prev=ev,ev=ev->next)
    {
      if (ev->id == id)
		{
		  if (prev)
			{
			  /* middle of end of list */
			  prev->next = ev->next;
			}
		  else /* !prev */
			{
			  /* beginning of list */
			  eventq_pending = ev->next;
			}

		  /* put event on free list */
		  ev->next = eventq_free;
		  eventq_free = ev;

		  /* return success */
		  return TRUE;
		}
    }
  /* not found */
  return FALSE;
}

void
eventq_service_events(tick_t now)
{
#ifdef SYNC_DVFS
  struct eventq_desc *ev = eventq_pending;
  struct eventq_desc *ev_prev = ev;
  while (ev)
    if(ev->when <= now) {
      if(ev_prev!=ev){ //the first event in the pending list
         ev_prev->next = ev->next;
         ev->next = eventq_free;
         eventq_free = ev;

         /* handle action */
         EXECUTE_ACTION(ev, now);

         ev = ev_prev->next;
         }
      else {
        ev_prev = ev->next;
        eventq_pending = ev->next;

        ev->next = eventq_free;
        eventq_free = ev;

        /* handle action */
        EXECUTE_ACTION(ev, now);

        ev = ev_prev;	
     }
    }
    else {
      if(ev_prev!=ev){
        ev_prev = ev_prev->next;
        ev = ev->next;
      }
      else
        ev = ev->next;
    }  

#else

//  while (eventq_pending && eventq_pending->when <= now)
  while (eventq_pending)
    {
      struct eventq_desc *ev = eventq_pending;

      sim_cycle = ev -> when;

      /* return the event record to the free list */
      eventq_pending = ev->next;
      ev->next = eventq_free;
      eventq_free = ev;
      EXECUTE_ACTION(ev, ev->when);
    }
#endif

}

void 
eventq_blow() {
  struct eventq_desc *prev, *ev;

  //  for (prev=NULL,ev=eventq_pending; ev; prev=ev,ev=ev->next) {
  ev = eventq_pending;
  prev = NULL;
  while (ev) {
    if (0) {
#if 0
      ev->data.callback2.fn == writeback_exec_loadstore || 
		ev->data.callback.fn == writeback_wakeup || 
		ev->data.callback.fn == issue_int_dec || 
		ev->data.callback.fn == issue_fp_dec ) {  
      if (prev) {
		/* middle of end of list */
		prev->next = ev->next;
		/* put event on free list */
		ev->next = eventq_free;
		eventq_free = ev;
		ev = prev;
      }
      else /* !prev */ {
		/* beginning of list */
		eventq_pending = ev->next;
		/* put event on free list */
		ev->next = eventq_free;
		eventq_free = ev;
		ev = eventq_pending;
      }
#endif
    }
    else {
      prev = ev;
      ev = ev->next;
    }
  }
}


void
eventq_dump(FILE *stream)
{
  struct eventq_desc *ev;

  if (!stream)
	stream = stderr;

  fprintf(stream, "Pending Events: ");
  for (ev=eventq_pending; ev; ev=ev->next)
    {
	  fprintf(stream, "@ %.0f:%s:",
			  (double)ev->when,
			  ev->action == EventSetBit ? "set bit"
			  : ev->action == EventClearBit ? "clear bit"
			  : ev->action == EventSetFlag ? "set flag"
			  : ev->action == EventAddOp ? "add operation"
			  : ev->action == EventCallback ? "call back"
			  : (abort(), ""));
	  switch (ev->action) {
	  case EventSetBit:
	  case EventClearBit:
		fprintf(stream, "0x%p, %d, %d",
				ev->data.bit.bmap, ev->data.bit.sz, ev->data.bit.bitnum);
		break;
	  case EventSetFlag:
		fprintf(stream, "0x%p, %d", ev->data.flag.pflag, ev->data.flag.value);
		break;
	  case EventAddOp:
		fprintf(stream, "0x%p, %d",
				ev->data.addop.summand, ev->data.addop.addend);
		break;
	  case EventCallback:
		fprintf(stream, "0x%p, %d",
				ev->data.callback.fn, ev->data.callback.arg);
		break;
	  default:
		panic("bogus event action");
	  }
	  fprintf(stream, " ");
    }
}
