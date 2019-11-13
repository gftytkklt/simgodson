/*
 * map.h - map stage interfaces
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
#ifndef MAP_H
#define MAP_H

#if 0

extern void map_stage_init(void);
extern void map_stage(void);

/* list of externs */
/* reorder queue */
extern int roq_ifq_size;
extern int roq_num;
extern int roq_head; 
extern int roq_tail;
extern struct inst_descript **roq;

/* branch queue,no real queue is needed */
extern int brq_ifq_size;
extern int brq_num;
extern int brq_head;
extern int brq_tail;
extern struct inst_descript **brq;

/* total integer rename register number */
extern int int_rename_reg_size;
/* used integer rename register number */
extern int int_rename_reg_num;

/* total floating-point rename register number */
extern int fp_rename_reg_size;
/* used floating-point rename register number */
extern int fp_rename_reg_num;

/* maximum instruction mapped per cycle */
extern int map_width;

extern counter_t sim_map_insn;

extern counter_t roq_count,roq_fcount;
extern counter_t lsq_count,lsq_fcount;
extern counter_t brq_count,brq_fcount;
extern counter_t intq_count,intq_fcount;
extern counter_t fpq_count,fpq_fcount;

extern void update_create_vector(struct inst_descript *rs);
extern void prepare_create_vector(int spec_level);

/*
 * RS_LINK defs and decls
 */

/* a reservation station link: this structure links elements of a ROQ
   reservation station list; used for ready instruction queue, event queue, and
   output dependency lists; each RS_LINK node contains a pointer to the ROQ
   entry it references along with an instance tag, the RS_LINK is only valid if
   the instruction instance tag matches the instruction ROQ entry instance tag;
   this strategy allows entries in the ROQ can be squashed and reused without
   updating the lists that point to it, which significantly improves the
   performance of (all to frequent) squash events */
struct RS_link {
  struct RS_link *next;			/* next entry in list */
  struct inst_descript *rs;		/* referenced ROQ resv station */
  unsigned int tag;			/* inst instance sequence number */
  union {
    tick_t when;			/* time stamp of entry (for eventq) */
    unsigned int seq;			/* inst sequence */
    int opnum;				/* input/output operand number */
  } x;
};

#define MAX_RS_LINKS  4096

/* RS link free list, grab RS_LINKs from here, when needed */
extern struct RS_link *rslink_free_list;

/* NULL value for an RS link */
#define RSLINK_NULL_DATA		{ NULL, NULL, 0 }
extern struct RS_link RSLINK_NULL; 

/* create and initialize an RS link */
#define RSLINK_INIT(RSL, RS)						\
  ((RSL).next = NULL, (RSL).rs = (RS), (RSL).tag = (RS)->tag)

/* non-zero if RS link is NULL */
#define RSLINK_IS_NULL(LINK)            ((LINK)->rs == NULL)

/* non-zero if RS link is to a valid (non-squashed) entry */
#define RSLINK_VALID(LINK)              ((LINK)->tag == (LINK)->rs->tag)

/* extra ROQ reservation station pointer */
#define RSLINK_RS(LINK)                 ((LINK)->rs)

#if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
   /* get a new RS link record */
# define RSLINK_NEW(DST, RS)						\
  { struct RS_link *n_link;						\
    if (!rslink_free_list)						\
      panic("out of rs links");						\
    n_link = rslink_free_list;						\
    rslink_free_list = rslink_free_list->next;				\
    n_link->next = NULL;						\
    n_link->rs = (RS); n_link->tag = n_link->rs->tag;			\
    (DST) = n_link;							\
  }
#else //Aggressive version follows
   /* get a new RS link record */
# define RSLINK_NEW(DST, RS)						\
  { struct RS_link *n_link;						\
    n_link = rslink_free_list;						\
    rslink_free_list = rslink_free_list->next;				\
    n_link->next = NULL;						\
    n_link->rs = (RS); n_link->tag = n_link->rs->tag;			\
    (DST) = n_link;							\
  }
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

/* free an RS link record */
#define RSLINK_FREE(LINK)						\
  {  struct RS_link *f_link = (LINK);					\
     f_link->rs = NULL; f_link->tag = 0;				\
     f_link->next = rslink_free_list;					\
     rslink_free_list = f_link;						\
  }

/* FIXME: could this be faster!!! */
/* free an RS link list */
#define RSLINK_FREE_LIST(LINK)						\
  {  struct RS_link *fl_link, *fl_link_next;				\
     for (fl_link=(LINK); fl_link; fl_link=fl_link_next)		\
       {								\
	 fl_link_next = fl_link->next;					\
	 RSLINK_FREE(fl_link);						\
       }								\
  }

#endif

#endif
