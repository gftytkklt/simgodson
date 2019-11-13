/*
 * fetch.h - fetch stage interfaces
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
#ifndef FETCH_H
#define FETCH_H

#if 0

/* default size of free list */
#define FETCH_FREE_LIST_SIZE  200

/* total input dependencies possible */
#define MAX_IDEPS               3
                                                                                
/* total output dependencies possible */
#define MAX_ODEPS               2

/* define the free list for storing instruction details */
struct inst_descript {
  /* fetch */
  md_inst_t IR;				/* inst register */
  md_addr_t regs_PC, regs_NPC, pred_PC,btarget;/* current PC, next PC, and 
					   predicted PCs,branch target*/
  enum md_opcode op;                    /* decoded instruction opcode */
  struct bpred_update_t dir_update;     /* predictor status */
  int pred_taken;                       /* predicted direction */
  int br_taken;                         /* real direction */
  int trap;                             /* this instruction should cause trap */
  tick_t time_stamp;                    /* at which cycle this is fetched */
  unsigned int seq;                     /* sequence number */
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
  int events;
  unsigned int ptrace_seq;            /* pipetrace sequence number */
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  /* decode */
  int in1,in2,in3,out1,out2;            /* source and destination register */
  int stack_recover_idx;                /* ras head for mis-specualtion recovery */
  int recover_inst;                     /* should we recover after this inst?*/
  int recover_PC;                       /* where to recover to */
  int bd;                               /* in delayslot ? */
  int mis_predict;                      /* mis-predict ? */
  int bht_op;                           /* conditional branch,not likely */
  int jr_op;                            /* jr or jalr */
  md_addr_t addr;                       /* calculated virtual address for ld/st*/
  int spec_level;                       /* which level wrong path? */

  /* save old value */
  struct {
    int t;
    int n;
    word_t addr;
    word_t l;
    sfloat_t f;
    dfloat_t d;
  } temp_save[3];

  int ti;                               /* total used save structure */
  int regn,regv;

  /* map */
  int roqid;                            /* reorder queue index */
  int lsqid;                            /* load/store queue index */
  int brqid;                            /* branch queue index */
  int bdrdy;                            /* for branch,delay slot in roq?*/

  int used_int_rename_reg;              /* number of int rename register used */
  int used_fp_rename_reg;		/* number of fp rename register used*/

  struct res_template *fu;     /* which unit this inst will use */

  /* instruction status */
  int mapped;                           /* register mapping done */
  int queued;                           /* operands ready and queued */
  int next_issue;                       /* chosen for issue? */
  int issued;                           /* operation is/was executing */
  int completed;                        /* operation has completed execution */
  int brcompleted;                      /* broperation has completed execution */

  unsigned int tag;                     /* roq instance tag,increase to squash operations */

  int in_intq;                         /* use integer issue queue ?*/
  int div_delay;

  /* Wattch: values of source operands and result operand used for AF generation */
  quad_t val_ra, val_rb, val_rc, val_ra_result;
  quad_t oldval_ra, oldval_rb, oldval_rc, oldval_ra_result;
      
  /* output operand dependency list, these lists are used to limit the number
   * of associative searches into the RUU when instructions complete and need
   * to wake up dependent insts 
   */
  int onames[MAX_ODEPS];                /* output logical names (NA=unused) */
  struct RS_link *odep_list[MAX_ODEPS]; /* chains to consuming operations */

  /* input dependent links, the output chains rooted above use these fields to
   * mark input operands as ready, when all these fields have been set
   * non-zero, the ROQ operation has all of its register operands, it may
   * commence execution as soon as all of its memory operands are known to be
   * read 
   */
  int idep_ready[MAX_IDEPS];            /* input operand ready? */

  int all_ready;                        /* all operand ready? */
  
  /* for istat */
  int  fetch_latency;                  /* latency for fetching this instruction */
  tick_t decode_stamp;                 /* at which cycle this is decoded */
  tick_t map_stamp;                    /* at which cycle this is mapped */
  tick_t issue_stamp;                  /* at which cycle this is issued */
  tick_t writeback_stamp;              /* at which cycle this is writeback */
  int icache_miss;                      /* icache miss met ? */
  int dcache_miss;                      /* dcache miss met ? */

  struct inst_descript * next;
};

/* non-zero if all register operands are ready, update with MAX_IDEPS */
#define OPERANDS_READY(RS)                                              \
  ((RS)->idep_ready[0] && (RS)->idep_ready[1] && (RS)->idep_ready[2])
                                                                                  
  /* non-zero if one register operands is ready, update with MAX_IDEPS */
#define ONE_OPERANDS_READY(RS)                                              \
    ((RS)->idep_ready[0] || (RS)->idep_ready[1])


/* Structure for buffering information about stalled i-fetches */
struct _istall_buf {
  int stall;
  int resume;
  md_inst_t inst;
};

enum icache_state { IREP_EMPTY, IREP_MISS, IREP_REFILL };

struct ireplace_buf {
  enum icache_state state;  /* status */
  unsigned int set;    /* refilling cache set */
  int way;             /* way to replace */
  int bitmap[8];       /* subblock status of refilling block*/
  md_addr_t paddr;     /* physical address of missing fetch */
};

/* list of externs */
extern int fetch_ifq_size;
extern int fetch_num;
extern int fetch_head; 
extern int fetch_tail;
extern struct inst_descript **fetch_data;
extern struct _istall_buf fetch_istall_buf;
extern int fetch_width;
extern int fetch_speed;
extern counter_t sim_fetch_insn;
extern counter_t ifq_count,ifq_fcount;

extern md_addr_t fetch_reg_PC;

extern struct ireplace_buf irepbuf;

/* function prototypes */
void fetch_stage_init(void);
void fetch_stage(void);
void fetch_init_inst_free_list(void);
struct inst_descript * fetch_get_from_free_list(void);
void fetch_return_to_free_list(struct inst_descript *);
void schedule_resume_ifetch(tick_t);
void fetch_resume_ifetch(tick_t, int);
void schedule_tlb_resume_ifetch(tick_t, cache_access_packet*, MSHR_STAMP_TYPE);

extern counter_t fetch_imiss_cycle,fetch_stall_cycle;

#endif

#endif
