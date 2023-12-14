/*
 * godson2_cpu.h - big struct to hold everything about a godson2 CPU
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
#ifndef __GODSON2_CPU_H__
#define __GODSON2_CPU_H__

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mips.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "cache.h"
#include "bpred.h"
#include "eventq.h"
#include "stats.h"
//#include "syscall.h"
#include "resource.h"
//#include "sim.h"
#include "ptrace.h"
//#include "noc.h"

struct router_t;

/******************************fetch stage***********************/

/* default size of free list */
#define FETCH_FREE_LIST_SIZE 	80 

/* total input dependencies possible */
#define MAX_IDEPS               3
                                                                                
/* total output dependencies possible */
#define MAX_ODEPS               2


typedef union {
  sword_t l;                          /* integer word view */
  sfloat_t f;                         /* single-precision floating point view */
  dfloat_t d;                         /* double-precision floating point view */
}regval_t;

/* define the free list for storing instruction details */
struct inst_descript {
  /* fetch */
  md_inst_t IR;				/* inst register */
  md_addr_t regs_PC, regs_NPC, pred_PC, btarget;/* current PC, next PC, and 
												   predicted PCs */
  md_addr_t target_PC;
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
  unsigned int in1,in2,in3,out1,out2;            /* source and destination register */
  int stack_recover_idx;                /* ras head for mis-specualtion recovery */
  int recover_inst;                     /* should we recover after this inst?*/
  //int recover_PC;                       /* where to recover to */
  md_addr_t recover_PC;
  int bd;                               /* in delayslot ? */
  int mis_predict;                      /* mis-predict ? */
  int bht_op;                           /* conditional branch,not likely */
  int jr_op;                            /* jr or jalr */
  md_addr_t addr;                       /* calculated virtual address for ld/st*/
  int spec_level;                       /* which level wrong path? */
  int div_delay;
  
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

  /* Wattch: values of source operands and result operand used for AF generation */
  quad_t val_ra, val_rb, val_rc, val_ra_result;

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

  struct inst_descript * next;

  struct {
    struct RS_link *next;			/* next entry in list */
    struct inst_descript *rs;		/* referenced ROQ resv station */
    unsigned int tag;			/* inst instance sequence number */
    union {
      tick_t when;			/* time stamp of entry (for eventq) */
      unsigned int seq;			/* inst sequence */
      int opnum;				/* input/output operand number */
    } x;
  }nextrs[MAX_ODEPS];

  struct {
    struct RS_link *next;			/* next entry in list */
    struct inst_descript *rs;		/* referenced ROQ resv station */
    unsigned int tag;			/* inst instance sequence number */
    union {
      tick_t when;			/* time stamp of entry (for eventq) */
      unsigned int seq;			/* inst sequence */
      int opnum;				/* input/output operand number */
    } x;
  }prevrs[MAX_ODEPS];

  //struct RS_link nextrs[MAX_ODEPS];
  //struct RS_link prevrs[MAX_ODEPS];
  regval_t idep_val[MAX_IDEPS];
  regval_t odep_val[MAX_ODEPS];
  int odep_ready[MAX_ODEPS];            /* input operand ready? */

  int iwin_flags;
  int stall_redirect;
  int last_commit_branch;
  int except;
  int flags;                  /*FAULT TYPE */
  int is_jump;
};

/* non-zero if all register operands are ready, update with MAX_IDEPS */
#define OPERANDS_READY(RS)                                              \
  ((RS)->idep_ready[0] && (RS)->idep_ready[1] && (RS)->idep_ready[2])
                                                                                  
/* non-zero if one register operands is ready, update with MAX_IDEPS */
#define ONE_OPERANDS_READY(RS)					\
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

/******************************decode stage***********************/

/*
 * configure the instruction decode engine
 */

#define DNA			(0)

/* general register dependence decoders */
#define DGPR(N)			(N)
#define DGPR_D(N)		((N) &~1)

/* floating point register dependence decoders */
#define DFPR_L(N)		(((N)+32)&~1)
#define DFPR_F(N)		(((N)+32)&~1)
#define DFPR_D(N)		(((N)+32)&~1)

/* miscellaneous register dependence decoders */
#define DHI			    (0+32+32)
#define DLO			    (1+32+32)
#define DFCC			(2+32+32)
#define DTMP			(3+32+32)

/* speculative memory hash table size, NOTE: this must be a power-of-two */
#define STORE_HASH_SIZE		32

/* speculative memory hash table definition, accesses go through this hash
   table when accessing memory in speculative mode, the hash table flush the
   table when recovering from mispredicted branches */
struct spec_mem_ent {
  struct spec_mem_ent *next;		/* ptr to next hash table bucket */
  md_addr_t addr;			/* virtual address of spec state */
  int level;                            /* written at which level? */
  unsigned int data[2];			/* spec buffer, up to 8 bytes */
};

/*
 * the create vector maps a logical register to a creator in the roq (and
 * specific output operand) or the architected register file (if RS_link
 * is NULL)
 */

/* an entry in the create vector */
struct CV_link {
  struct inst_descript *rs;               /* creator's reservation station */
  int odep_num;                         /* specific output operand */
};

/******************************map stage***********************/

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


/* NULL value for an RS link */
#define RSLINK_NULL_DATA		{ NULL, NULL, 0 }
extern struct RS_link RSLINK_NULL; 

/* create and initialize an RS link */
#define RSLINK_INIT(RSL, RS)									\
  ((RSL).next = NULL, (RSL).rs = (RS), (RSL).tag = (RS)->tag)

/* non-zero if RS link is NULL */
#define RSLINK_IS_NULL(LINK)            ((LINK)->rs == NULL)

/* non-zero if RS link is to a valid (non-squashed) entry */
#define RSLINK_VALID(LINK)              ((LINK)->tag == (LINK)->rs->tag)

/* extra ROQ reservation station pointer */
#define RSLINK_RS(LINK)                 ((LINK)->rs)

#if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
/* get a new RS link record */
# define RSLINK_NEW(DST, RS)							\
  { struct RS_link *n_link;								\
    if (!st->rslink_free_list)							\
      panic("out of rs links");							\
    n_link = st->rslink_free_list;						\
    st->rslink_free_list = st->rslink_free_list->next;	\
    n_link->next = NULL;								\
    n_link->rs = (RS); n_link->tag = n_link->rs->tag;	\
    (DST) = n_link;										\
  }
#else //Aggressive version follows
/* get a new RS link record */
# define RSLINK_NEW(DST, RS)							\
  { struct RS_link *n_link;								\
    n_link = st->rslink_free_list;						\
    st->rslink_free_list = st->rslink_free_list->next;	\
    n_link->next = NULL;								\
    n_link->rs = (RS); n_link->tag = n_link->rs->tag;	\
    (DST) = n_link;										\
  }
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

/* free an RS link record */
#define RSLINK_FREE(LINK)						\
  {  struct RS_link *f_link = (LINK);			\
	f_link->rs = NULL; f_link->tag = 0;			\
	f_link->next = st->rslink_free_list;		\
	st->rslink_free_list = f_link;				\
  }

/* FIXME: could this be faster!!! */
/* free an RS link list */
#define RSLINK_FREE_LIST(LINK)							\
  {  struct RS_link *fl_link, *fl_link_next;			\
	for (fl_link=(LINK); fl_link; fl_link=fl_link_next)	\
	  {													\
		fl_link_next = fl_link->next;					\
		RSLINK_FREE(fl_link);							\
	  }													\
  }

/******************************issue stage***********************/

/* TODO: we need to find a way to efficiently performance issue queue
 * operations. 
 *   1. age increasment
 *   2. find issue queue slots for new instructions
 *   3. select instructions to issue
 */
struct issue_queue {
  int valid;
  int age;
  struct inst_descript *rs;
};

/******************************writeback stage***********************/
/******************************commit stage**************************/

/******************************lsq cache2mem*************************/

#define DWORD_MATCH(a1,a2) (((a1)|0x7) == ((a2)|0x7))
/* FIXME: use correct block size */
#define BLOCK_MATCH(a,b) (((a) & ~0x1f) == ((b) & ~0x1f))

enum lsq_state { LSQ_EMPTY,LSQ_ENTER,LSQ_ISSUE,LSQ_DCACHE,LSQ_DTAGCMP,LSQ_READCACHE,LSQ_WRITEBACK,LSQ_COMMIT,LSQ_DELAY1 };

struct load_store_queue {
  struct inst_descript *rs;
  enum lsq_state state;
  int tag;
  int op;
  int op_load;
  int op_store;
  int op_ll;
  int op_sc;
  md_addr_t addr;
  md_addr_t paddr;
  struct cache_blk *blk;
  int req;
  int ex;
  int set;
  //struct DCacheSet * set;
  int way;
  int cachehit;
  unsigned char byterdy;
  unsigned char bytemask;
  //unsigned char fwdvalid;
  int fetching;
  //int hit_assoc;
  int loadspec;
  //char data[8];
  //unsigned char offindw; /*offset in a DoubleWord ,0-7*/
  //uint tlbFlavor;
  //uint  size;

  //int dtlbmiss;
};

/*
  enum miss_state {
  MQ_EMPTY,MQ_MISS,MQ_VCMISS,MQ_VCHIT,MQ_MEMREF,MQ_DELAY1,MQ_DELAY2,MQ_DELAY3
  };
*/

enum miss_state {
  MQ_EMPTY = 0, MQ_L1_MISS = 1, MQ_READ_L2 = 2, MQ_MODIFY_L1 = 3 , MQ_REPLACE_L1 = 4, MQ_EXTRDY = 5 ,MQ_REFILL_L1 = 6, MQ_L2_MISS = 7, MQ_MEMREF = 8, MQ_REFILL_L2 = 9, MQ_DELAY1 = 10, MQ_DELAY2 = 11, MQ_DELAY3 = 12,MQ_STATE_NUM = 13
};

struct miss_queue {
  enum miss_state state;
  int  set;
  md_addr_t paddr;
  int  inst;
  int  memcnt;
  int  cachecnt;
  /* attributes added to support directory cache coherence */
  int  cpuid;
  int  req;
  int  ack;
  int  cache_status;/* record L1 cache status for ack. */
  int  qid;
  int  data_directory[MAX_CPUS];
  int  inst_directory[MAX_CPUS];
  int  data_intervention_sent[MAX_CPUS];
  int  inst_intervention_sent[MAX_CPUS];
  int  data_ack_received[MAX_CPUS];
  int  inst_ack_received[MAX_CPUS];
  int  intervention_type;
  struct cache_blk *L2_changed_blk;
  struct cache_blk *L2_replace_blk;
  md_addr_t L2_replace_paddr;/* is cache block size align */
  int  L2_replace_way;
  int  wait_for_reissue;
  int  wait_for_response;/* 0 indicates empty, 1 indicates that L1 and L2 is not coherent and directory is waiting for response, 2 indicates response has arrived and should reprobe L2 cache */
  int  invn_match_i1;
  int  invn_match_i2;
  int read_local_l2;
  int modify_l1; /* 1 indicates the external intervention request has not modified the l1 cache status. */
  int wtbkq_checked; /* 1 indicates wtbkq has been checked for conflict write back request. */
  int conflict_checked;
  int memread_sent;/* 1 indicates when L2 cache miss occured, a memory read request has been sent out, the missq state can be converted to MQ_MEMREF if all the invalidates to L1 cache receive responses.*/
  int memwrite_sent;/* indicates whether L2 cache block has been write back to memory. */
  int scache_refilled;/* indicates L2 cache has been refilled. */
  int lsconten;
};

struct writeback_queue {
  int valid;
  int cache_status;
  md_addr_t paddr;
  /* attributes added to support directory cache coherence */
  int cpuid;
  int req;
  int inst;
  int intervention; /* indicates whether this writeback is invoked by an intervention request, if so, the write back will be definitely write into missq instead of the l2 cache. */
};

struct refill_packet {
  int valid;
  int replace;
  int intervention;
  int set;
  md_addr_t paddr;
  int cnt;
  int req; /* initial request kind to determine return block state */
  int missqid; /* when this refill is invoked by an external request, record the corresponding missq entry for later ack to write in. */
  int ack; /* when this refill is invoked by an external request, record whether the corresponding ack is with data.*/
  int cache_status; /* when this refill is invoked by an external request, record the L1 cache status for directory to decide whether the request need to be re-sent.*/
  struct cache_blk *blk;
  struct refill_packet *next;
};

struct memory_queue {
  int valid;
  int read;
  int qid;
  int count;
  struct memory_queue *next;
};

#define REFILL_FREE_LIST_SIZE   32

/* Maximum total size of new thread, including text,data,heap and stack*/
#define NEW_THREAD_TOTAL_SIZE   ((unsigned long)(1<<MD_LOG_MEM_SIZE)/total_cpus)   /* 128MB */

#define VALID_PADDR(paddr)      ( paddr >= 0 && paddr < (1<<MD_LOG_MEM_SIZE) )

#define PADDR_OWNER(paddr)      ( ( paddr >> ( MD_LOG_MEM_SIZE - log_base2(total_cpus) ) ) & (total_cpus - 1) )
	
#define BRK_START	0x00400000


/* sampling support */

/* enum that indicates whether the simulator is currently warming, measuring, or fast-forwarding */
enum simulator_state_t {
  NOT_STARTED,
  MEASURING,
  WARMING,
  DRAINING,
  FAST_FORWARDING
};

/* Sampling standard deviation variables */
struct stddev_entry_t {
  int cycles[1024];
  struct stddev_entry_t* next;
};

struct simoo_stats_t {
  counter_t sim_meas_insn;
  tick_t sim_meas_cycle;
#if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
  counter_t sim_slip;
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  counter_t sim_total_insn;
  counter_t sim_num_refs;
  counter_t sim_total_refs;
  counter_t sim_num_loads;
  counter_t sim_total_loads;
  counter_t sim_num_branches;
  counter_t sim_total_branches;
  counter_t ifq_count;
  counter_t ifq_fcount;
  counter_t roq_count;
  counter_t roq_fcount;
  counter_t lsq_count;
  counter_t lsq_fcount;
};


/*****************************cpu structure************************/

struct godson2_cpu {
#ifdef SIMOS_GODSON
  /* point to simos cpu state structure*/
  void *owner;
#endif

  int cpuid;
  int active;
  char *stdin_addr;

  // sdbbp
  int sdbbp; // nemu ebreak

  /* fetch queue */
  int fetch_num;
  int fetch_head; 
  int fetch_tail;
  struct inst_descript **fetch_data;

  /* stop fetch control */
  struct _istall_buf fetch_istall_buf;

  unsigned int inst_seq;

#if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
  /* pipetrace instruction sequence counter */
  unsigned int ptrace_seq;
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  /* The instruction descriptor free list */
  struct inst_descript *fetch_inst_free_list, *fetch_inst_free_tail;

  /* stats */
  counter_t sim_fetch_insn;
  counter_t ifq_count,ifq_fcount;

  /* next pc to fetch */
  md_addr_t fetch_reg_PC;
  md_addr_t committing_PC;

  /* icache replace control */
  struct ireplace_buf irepbuf;

  /* decode stage */

  int decode_num;
  int decode_head; 
  int decode_tail;
  struct inst_descript **decode_data;
  counter_t sim_decode_insn;

  /* which level of mis-speculation we are on? */
  int spec_level ;

  int is_taken_delayslot ;  
  int entering_spec; 
  int need_to_correct_pc; 
  int last_is_branch;
  struct inst_descript *last_non_spec_rs;

  md_addr_t recover_PC;
  md_addr_t correct_PC; 

  /* integer register file */
  /* each level has its register file */
  md_gpr_t *spec_regs_R;

  /* floating point register file */
  /* each level has its register file */
  md_fpr_t *spec_regs_F;

  /* miscellaneous registers */
  /* each level has its register file */
  md_ctrl_t *spec_regs_C;

  /* speculative memory hash table */
  struct spec_mem_ent *store_htable[STORE_HASH_SIZE];

  /* speculative memory hash table bucket free list */
  struct spec_mem_ent *bucket_free_list;

  /* the create vectors,no copy-on-write because of multiple-level mis-speculation support */
  struct CV_link **create_vector;
  tick_t **create_vector_rt;

  /* map stage */

  struct RS_link *rslink_free_list;

  /* reorder queue */
  int roq_num;
  int roq_head; 
  int roq_tail;
  struct inst_descript **roq;

  /* branch queue,no real queue is needed */
  int brq_num;
  int brq_head;
  int brq_tail;
  struct inst_descript **brq;

  /* used integer rename register number */
  int int_rename_reg_num;

  /* used floating-point rename register number */
  int fp_rename_reg_num;

  counter_t sim_map_insn;
  counter_t roq_count,roq_fcount;
  counter_t brq_count,brq_fcount;

  /* issue stage */
  int issue_width;

  /* integer issue queue */
  int int_issue_num;
  struct issue_queue *int_issue;
  counter_t intq_count,intq_fcount;

  /* floating-point issue queue */
  int fp_issue_num;
  struct issue_queue *fp_issue;
  counter_t fpq_count,fpq_fcount;

  counter_t sim_issue_insn;

  struct RS_link **ready_queue;
  struct RS_link **next_issue_item;

  /* issued instruction for each function unit */
  counter_t *fu_inst_num;

  /* writeback stage */
  counter_t sim_writeback_insn;
  counter_t sim_brbus_count;
  counter_t sim_brerr_count;
  counter_t sim_bht_count;
  counter_t sim_bhterr_count;
  counter_t sim_jr_count;
  counter_t sim_jrerr_count;

  /* pending event queue, sorted from soonest to latest event (in time), NOTE:
	 RS_LINK nodes are used for the event queue list so that it need not be
	 updated during squash events */
  struct RS_link *event_queue;

  /* commit stage */
  counter_t sim_commit_insn;
  counter_t sim_missspec_load;
  /* cycles between fetch/commit */
  tick_t sim_slip;

  /* memory hierarchy */

  /* load store queue */
  struct load_store_queue *lsq;
  int lsq_head1,lsq_head,lsq_tail,lsq_num;

  /* index of lsq item in certain states,at most one item for each of these */
  int lsq_issuei,lsq_dcachei,lsq_dtagcmpi,lsq_missi,lsq_wtbki;

  /* refill/replace operation in dcache/dtagcmp stage */
  struct refill_packet *lsq_refill,*lsq_dcache_refill,*lsq_dtagcmp_refill;

  /* stats for load/store queue */
  counter_t sim_loadcnt,sim_storecnt,sim_loadmisscnt,sim_storemisscnt;
  counter_t sim_loadfwdcnt,sim_storefwdcnt,sim_ldwtbkdelaycnt;
  counter_t lsq_count,lsq_fcount;
  /* for dcache replace */
  int replace_delay;

  /* for dcache refill, when a dcache refill completed, after refill_delay could an external intervention request can be processed. */
  int refill_delay;

  /* cache2mem */

  counter_t icache_read_low_level_count;
  counter_t dcache_read_low_level_count;
  counter_t noc_read_count;
  counter_t noc_write_count;
  counter_t memq_read_count;
  counter_t memq_write_count;
  counter_t memq_busy_count;

  /* router struct of per cpu core */
  struct router_t* router;
 
  /* miss queue and writeback queue */
  struct miss_queue *missq;
  int missq_num;
  int lasti[MQ_STATE_NUM];
 
  struct writeback_queue *wtbkq;
  int wtbkq_replacei;
  int wtbkq_num;
  int wtbkq_lasti;

  /* external intervention queue for cache coherence */
  struct miss_queue *extinvnq;

  /* struct for refill */
  struct refill_packet *refill_free_list;

  int refilltag;
  int refilli;

  /* memory access queue */
  struct memory_queue memq;
  struct memory_queue *memq_head,*memq_free_list;
  int memq_num;

  /* branch predictor */
  struct bpred_t *pred;
  int ghr_valid;
  int ghr_predict;

  /* Variables defined in cache.c */
  struct cache *icache;
  struct cache *dcache;
  struct cache *cache_dl2;
  struct cache_set vbuf;
  counter_t victim_buf_hits;
  counter_t victim_buf_misses;
  int perfectl2;

  /* instruction TLB */
  struct cache *itlb ;
  /* data TLB */
  struct cache *dtlb ;

  /* architecture registers */
  struct regs_t regs;

  /* sampling state */
  enum simulator_state_t simulator_state;
  /* inst count for status switch,when reached,take proper 
   * actions */
  counter_t switchpoint;
  /* finished draining */
  int done_draining;
  /* remember this to help switch */
  enum md_opcode last_fastfwd_op;
	
  struct stddev_entry_t* stddev_head; 
  struct stddev_entry_t* stddev_current;
  int stddev_index;

  double standardDeviation;
  double percentageErrorInterval;  /* not multiplied by 100%, so 3% is 0.03 */
  int recommendedK;  /* K recommended to achieve +/- 3%*/

  struct simoo_stats_t simoo_stats_backup;

  md_addr_t last_dl1_tag;
  md_addr_t dl1_tagset_mask;
  md_addr_t last_dtlb_tag;
  md_addr_t dtlb_tagset_mask;
  int last_dl1_block_dirty;

  md_addr_t last_il1_tag;
  md_addr_t il1_tagset_mask;
  md_addr_t last_itlb_tag;
  md_addr_t itlb_tagset_mask;

  /* used by mips.def */
  /* flag of the delay slot */
  int is_jump;

  /* flag of branch likely delay slot annulling*/
  int is_annulled;

  md_addr_t target_PC;

  /* 
   * simulator stats
   */
  counter_t sim_pop_insn;     /* execution instruction counter */
  counter_t sim_total_insn;   /* total number of instructions executed */
  counter_t sim_detail_insn;  /* total number of instructions measured */
  counter_t sim_meas_insn;    /* total number of instructions measured */
  tick_t    sim_meas_cycle;   /* measured cycle */
  counter_t sim_num_refs;     /* total number of memory references committed */
  counter_t sim_total_refs;   /* total number of memory references executed */
  counter_t sim_num_loads;    /* total number of loads committed */
  counter_t sim_total_loads;  /* total number of loads executed */ 
  counter_t sim_num_stores;   /* total number of stores committed */ 
  counter_t sim_total_stores; /* total number of stores executed */ 
  counter_t sim_num_branches; /* total number of branches committed */
  counter_t sim_total_branches; /* total number of branches executed */

  counter_t sim_sample_size;    /* sample size*/
  counter_t sim_sample_period;  /* period of each sample */
	
  double sim_inst_rate;
  double sim_pop_rate;
	
  double sim_IPC;
  double total_IPC;
  counter_t total_packets;
  double sim_CPI;
  double sim_exec_BW;
  double sim_IPB;
	
  double roq_occupancy;
  double roq_rate;
  double roq_latency;
  double roq_full;
	
  double brq_occupancy;
  double brq_rate;
  double brq_latency;
  double brq_full;
	
  double lsq_occupancy;
  double lsq_rate;
  double lsq_latency;
  double lsq_full;
	
  double intq_occupancy;
  double intq_rate;
  double intq_latency;
  double intq_full;
	
  double fpq_occupancy;
  double fpq_rate;
  double fpq_latency;
  double fpq_full;
	
  double avg_sim_slip;
  double sim_br_miss;
  double sim_bht_miss;
  double sim_jr_miss;
  
  double dcache_miss;
  double dcache_miss_rate;
  double icache_miss;
  double icache_miss_rate;
  double memq_busy_rate;

  /* cache coherence stats */
  counter_t L1_accesses;
  counter_t L1_stores;
  counter_t L1_hits;
  counter_t L1_misses;
  double    L1_miss_rate;
  counter_t L1_writebacks;
  counter_t L2_hits;
  counter_t L2_misses;
  double    L2_miss_rate;
  counter_t L2_writebacks;
  counter_t L1_exclusive_to_modified_changes;
  counter_t L1_read_shared_requests;
  counter_t L1_read_exclusive_requests;
  counter_t L1_upgrade_requests;
  counter_t L1_external_invalidations;
  counter_t L1_external_intervention_shareds;
  counter_t L1_external_intervention_exclusives;
  //counter_t shared_data_responses;
  //counter_t exclusive_data_responses;
  //counter_t exclusive_data_returns;

  /* counters for load mispeculation */
  counter_t sim_load_mispec_rep;
  counter_t sim_load_mispec_inv;
  counter_t sim_load_store_contention;

  /* counters added for our power model */
  counter_t bht_fetch_access;
  counter_t bht_wb_access;
  counter_t btb_fetch_access;
  counter_t btb_wb_access;
  counter_t decode_access;
  counter_t fix_map_access;
  counter_t fp_map_access;
  counter_t fix_wb_access;
  counter_t fp_wb_access;
  counter_t fix_issue_access;
  counter_t fp_issue_access;
  counter_t fix_commit_access;
  counter_t fp_commit_access;
  counter_t fix_add_access;
  counter_t fix_br_access;
  counter_t fix_mem_access;
  counter_t fix_mult_access;
  counter_t fix_div_access;
  counter_t fp_add_access;
  counter_t fp_br_access;
  counter_t fp_cmp_access;
  counter_t fp_cvt_access;
  counter_t fp_mult_access;
  counter_t fp_div_access;
  counter_t fp_mdmx_access;
  counter_t commit_access;
  counter_t lsq_map_access;
  counter_t lsq_wb_access;
  counter_t lsq_commit_access;
  counter_t lsq_store_commit_access;
  counter_t icache_fetch_access;
  counter_t icache_refill_access;
  counter_t dcache_refill_access;
  counter_t replace_access;
  counter_t missq_access;
  counter_t wtbkq_access;
  counter_t cache_dl2_fetch_access;
  counter_t cache_dl2_refill_access;
  counter_t cache_dl2_wtbk_access;
  counter_t memq_access;

  counter_t avg_bht_fetch_access;
  counter_t avg_bht_wb_access;
  counter_t avg_btb_fetch_access;
  counter_t avg_btb_wb_access;
  counter_t avg_decode_access;
  counter_t avg_fix_map_access;
  counter_t avg_fp_map_access;
  counter_t avg_fix_wb_access;
  counter_t avg_fp_wb_access;
  counter_t avg_fix_issue_access;
  counter_t avg_fp_issue_access;
  counter_t avg_fix_commit_access;
  counter_t avg_fp_commit_access;
  counter_t avg_fix_add_access;
  counter_t avg_fix_br_access;
  counter_t avg_fix_mem_access;
  counter_t avg_fix_mult_access;
  counter_t avg_fix_div_access;
  counter_t avg_fp_add_access;
  counter_t avg_fp_br_access;
  counter_t avg_fp_cmp_access;
  counter_t avg_fp_cvt_access;
  counter_t avg_fp_mult_access;
  counter_t avg_fp_div_access;
  counter_t avg_fp_mdmx_access;
  counter_t avg_commit_access;
  counter_t avg_lsq_map_access;
  counter_t avg_lsq_wb_access;
  counter_t avg_lsq_commit_access;
  counter_t avg_lsq_store_commit_access;
  counter_t avg_icache_fetch_access;
  counter_t avg_icache_refill_access;
  counter_t avg_dcache_refill_access;
  counter_t avg_replace_access;
  counter_t avg_missq_access;
  counter_t avg_wtbkq_access;
  counter_t avg_cache_dl2_fetch_access;
  counter_t avg_cache_dl2_refill_access;
  counter_t avg_cache_dl2_wtbk_access;
  counter_t avg_memq_access;

#ifdef POWER_STAT
#define NUM_TYPES 4
  /* power statistics struct pointer */
  struct power_stat *cpu_power;
  struct power_stat *bpred_power;
  struct power_stat *decode_power;
  struct power_stat *rename_power;
  struct power_stat *window_power;
  struct power_stat *regfile_power;
  struct power_stat *ialu_power;
  struct power_stat *falu_power;
  struct power_stat *lsq_power;
  struct power_stat *roq_power;
  struct power_stat *icache_power;
  struct power_stat *itlb_power;
  struct power_stat *dcache_power;
  struct power_stat *dtlb_power;
  struct power_stat *cache_dl2_power;
  struct power_stat *cache2mem_power;
  struct power_stat *router_power;
  struct power_stat *input_buffer_power[NUM_TYPES];
  struct power_stat *arbiter_power[NUM_TYPES];
  struct power_stat *crossbar_power[NUM_TYPES];
  struct power_stat *link_power[NUM_TYPES];
  struct power_stat *clocktree_power;
  struct power_stat *IOpad_power;

  /* power statistics */
  double avg_cpu_power_total;
  double avg_bpred_power_total;
  double avg_decode_power_total;
  double avg_rename_power_total;
  double avg_window_power_total;
  double avg_regfile_power_total;
  double avg_ialu_power_total;
  double avg_falu_power_total;
  double avg_lsq_power_total;
  double avg_roq_power_total;
  double avg_icache_power_total;
  double avg_itlb_power_total;
  double avg_dcache_power_total;
  double avg_dtlb_power_total;
  double avg_cache_dl2_power_total;
  double avg_cache2mem_power_total;
  double avg_input_buffer_power_total[NUM_TYPES];
  double avg_arbiter_power_total[NUM_TYPES];
  double avg_crossbar_power_total[NUM_TYPES];
  double avg_link_power_total[NUM_TYPES];
  double avg_router_power_total;
  double avg_IOpad_power_total;
  double avg_clocktree_power_total;

  double avg_cpu_power_dynamic;
  double avg_bpred_power_dynamic;
  double avg_decode_power_dynamic;
  double avg_rename_power_dynamic;
  double avg_window_power_dynamic;
  double avg_regfile_power_dynamic;
  double avg_ialu_power_dynamic;
  double avg_falu_power_dynamic;
  double avg_lsq_power_dynamic;
  double avg_roq_power_dynamic;
  double avg_icache_power_dynamic;
  double avg_itlb_power_dynamic;
  double avg_dcache_power_dynamic;
  double avg_dtlb_power_dynamic;
  double avg_cache_dl2_power_dynamic;
  double avg_cache2mem_power_dynamic;
  double avg_input_buffer_power_dynamic[NUM_TYPES];
  double avg_arbiter_power_dynamic[NUM_TYPES];
  double avg_crossbar_power_dynamic[NUM_TYPES];
  double avg_link_power_dynamic[NUM_TYPES];
  double avg_router_power_dynamic;
  double avg_IOpad_power_dynamic;
  double avg_clocktree_power_dynamic;

  double avg_cpu_power_clock;
  double avg_bpred_power_clock;
  double avg_decode_power_clock;
  double avg_rename_power_clock;
  double avg_window_power_clock;
  double avg_regfile_power_clock;
  double avg_ialu_power_clock;
  double avg_falu_power_clock;
  double avg_lsq_power_clock;
  double avg_roq_power_clock;
  double avg_icache_power_clock;
  double avg_itlb_power_clock;
  double avg_dcache_power_clock;
  double avg_dtlb_power_clock;
  double avg_cache_dl2_power_clock;
  double avg_cache2mem_power_clock;
  double avg_input_buffer_power_clock[NUM_TYPES];
  double avg_arbiter_power_clock[NUM_TYPES];
  double avg_crossbar_power_clock[NUM_TYPES];
  double avg_link_power_clock[NUM_TYPES];
  double avg_router_power_clock;
  double avg_IOpad_power_clock;
  double avg_clocktree_power_clock;
#undef NUM_TYPES
#endif

  /* per cpu stats database */
  struct stat_sdb_t *sdb;
  struct stat_stat_t *l1dmiss_dist;
  
  /* functional unit resource pool */
  struct res_pool *fu_pool;
  /* for fu allocation */
  unsigned int alloc_stamp;

  /* dead lock detection */
  int no_commit;

  /* variables for loading program into target machine memory */
  md_addr_t    ld_text_base;  /* program text (code) segment base */
  unsigned int ld_text_size;  /* program text (code) size in bytes */
  md_addr_t    ld_data_base;  /* program initialized data segment base */
  unsigned int ld_data_size;  /* program initialized ".data" and uninitialized ".bss" size in bytes */
  md_addr_t    ld_brk_point;  /* top of the data segment */
  md_addr_t    ld_stack_base; /* program stack segment base (highest address in stack) */
  unsigned int ld_stack_size; /* program initial stack size */
  md_addr_t    ld_stack_min;  /* lowest address accessed on the stack */
  md_addr_t    mmap_base;  	 /* program mmap segment base */
  unsigned int mmap_size;  	 /* program mmap size in bytes */
  char        *ld_prog_fname; /* program file name */
  md_addr_t    ld_prog_entry; /* program entry point (initial PC) */
  md_addr_t    ld_environ_base;  /* program environment base address address */
  //int          ld_target_big_endian;  /* target executable endian-ness, non-zero if big endian */
  //md_addr_t    mem_brk_point; /* top of the data segment */
  //md_addr_t    mem_stack_min; /* lowest address accessed on the stack */
 
  struct mem_t *mem;
 
  int exception_pending;
  int stall_fetch;            /* If instruction fetch stalled */
  int branch_dly;             /* In branch delay slot         */
  int branch_likely;          /* Ditto for branch likely inst's */
  int stall_fpc;              /* If stall due to FP control write */
  int stall_branch;           /* Stalled waiting for branch   */
  int stall_thread;           /* Stalled waiting for branch - */
  /* no free threads */
  int stall_icache;           /* Problem fetching inst. from cache */
  int icache_stall_reason;    /* Cause of icache stall */
  int stall_except;           /* Stall due to exception       */
  int stall_itlbmiss;         /* Stall due to ITLB miss       */
  int stall_sys;              /* Stall due to system call     */
  int stall_sc;               /* Stall due to an SC           */
  int stall_cp0;              /* Stall due to an CP0 inst     */
  int stall_redirect;

  md_gpr_t temp_regs_R;
  md_fpr_t temp_regs_F;
  md_ctrl_t temp_regs_C;
  int refill_dcache;
  int irefill_busy;
  int except;
  int last_commit_branch;
  int drefill_busy;
  md_addr_t last_commit_PC; 

  int local_active;

  double period;  //qffan --2006-11-01
  counter_t cycles; //qffan --2006-11-02
  counter_t old_cycles;//qffan --2006-12-13
  double old_sim_cycle;
  int start_dvfs;
  long long int old_sim_commit_insn;//qffan --2006-12-13
  int UpperEndstopCounter;//qffan --2006-12-13
  int LowerEndstopCounter;//qffan --2006-12-13
  double PrevIPC;
  double QueueUtilization;
  double PrevQueueUtilization;

//for asynchronous DVFS
#ifdef ASYNC_DVFS
  double next_period;  //qffan --2006-11-01
  
  int voltage_changing;
  int freq_changing;
  
  int freq;
  int next_freq;
#endif

#ifdef SYNC_DVFS
//for synchronous DVFS
 int duty; /* qffan -2006-11-08 duty: range: 1 to 8 */
 int next_duty;
//double period;  //qffan --2006-11-01
 
 double voltage;
 int voltage_downgrading;
 int voltage_upgrading;
#endif
};

/* function prototypes */
/* fetch stage */

extern void fetch_stage_init(struct godson2_cpu *st);
extern void fetch_stage(struct godson2_cpu *st);

extern void fetch_init_inst_free_list(struct godson2_cpu *st);
extern struct inst_descript * fetch_get_from_free_list(struct godson2_cpu *st);
extern void fetch_return_to_free_list(struct godson2_cpu *st,struct inst_descript *);

extern void schedule_resume_ifetch(tick_t t);
extern void fetch_resume_ifetch(tick_t t , int arg);

extern void schedule_tlb_resume_ifetch(tick_t, cache_access_packet*, MSHR_STAMP_TYPE);


/* decode stage */
extern void decode_stage_init(struct godson2_cpu *st);
extern void decode_stage(struct godson2_cpu *st);
extern void tracer_recover(struct godson2_cpu *st,int level,md_addr_t recover_pc);
//extern void tracer_recover(struct godson2_cpu *st,md_addr_t recover_pc);

/* map */
extern void map_stage_init(struct godson2_cpu *st);
extern void map_stage(struct godson2_cpu *st);
extern void update_create_vector(struct godson2_cpu *st,struct inst_descript *rs);
extern void prepare_create_vector(struct godson2_cpu *st,int spec_level);

/* issue */
extern void issue_stage_init(struct godson2_cpu *st);
extern void issue_stage(struct godson2_cpu *st);
/*
  extern void int_issue_enqueue(struct inst_descript *);
  extern void fp_issue_enqueue(struct inst_descript *);
*/
extern void issue_enqueue(struct godson2_cpu *st,struct inst_descript *rs);
extern void readyq_enqueue(struct godson2_cpu *st,struct inst_descript *rs);		/* RS to enqueue */
extern void release_fu(struct godson2_cpu *st);

/* writeback */
extern void writeback_stage_init(struct godson2_cpu *st);
extern void writeback_stage(struct godson2_cpu *st);
extern void eventq_queue_event(struct godson2_cpu *st,struct inst_descript *rs, tick_t when);
extern void check_brq(struct godson2_cpu *st);

/* commit */
extern void commit_stage_init(struct godson2_cpu *st);
extern int commit_stage(struct godson2_cpu *st);

/* memory hierarchy */
extern void lsq_enqueue(struct godson2_cpu *st,struct inst_descript *rs);
extern void lsq_issue(struct godson2_cpu *st,struct inst_descript *rs);
extern void lsq_init(struct godson2_cpu *st);
extern void lsq_stage(struct godson2_cpu *st);
extern void lsq_cancel_one(struct godson2_cpu *st,int i);

extern void cache2mem_init(struct godson2_cpu *st);
extern void cache2mem(struct godson2_cpu *st);
extern void check_memq(struct godson2_cpu *st);
extern void wtbkq_enter(struct godson2_cpu *st,int cpuid,int req,md_addr_t paddr,int inst,int w,int intervention);
extern int intervention_shared(struct godson2_cpu *st, md_addr_t paddr,struct cache_blk** pblk,int* cache_status);
extern int intervention_exclusive(struct godson2_cpu *st, md_addr_t paddr,struct cache_blk** pblk,int* cache_status);
extern void invalidate(struct godson2_cpu *st, md_addr_t paddr,struct cache_blk** pblk,int* cache_status);
extern int find_missq_empty_entry(struct godson2_cpu *st);
extern int find_missq_two_empty_entry(struct godson2_cpu *st);
//extern int find_extinvnq_empty_entry(struct godson2_cpu *st);
//extern int find_wtbkq_empty_entry(struct godson2_cpu *st);

extern struct refill_packet *refill_get_free_list(struct godson2_cpu *st);
extern void refill_return_to_free_list(struct godson2_cpu *st,struct refill_packet *temp);

extern void clear_access_stats(struct godson2_cpu *st);
extern void update_power_stats(struct godson2_cpu *st);
//extern void calculate_power(power_result_type*);

/* sampling */
/* updates standardDeviation */
extern void doStatistics(struct godson2_cpu *st);

extern void simoo_backup_stats(struct godson2_cpu *st);
extern void simoo_restore_stats(struct godson2_cpu *st);
extern void switch_to_fast_forwarding(struct godson2_cpu *st);
extern void switch_to_warming(struct godson2_cpu *st);
extern void start_measuring(struct godson2_cpu *st);
extern void stop_measuring(struct godson2_cpu *st);
extern void run_fast(struct godson2_cpu *st,counter_t count);

/* resource */
extern void resource_init(struct godson2_cpu *st);
extern void release_fu(struct godson2_cpu *st);
extern struct res_template * my_res_get(struct godson2_cpu *st,int class);
/* allocation proper function unit for op with given class */
extern struct res_template * res_alloc (struct godson2_cpu*st, int class);
/* total function unit number */
extern int res_get_fu_number(struct godson2_cpu *st);

extern int godson_cycle_once(struct godson2_cpu *st);
extern int run_fast_once(struct godson2_cpu *st);

#ifdef SIMOS_GODSON
int ss_init(int argc, char **argv, char **envp);
#endif

extern void cpu_init(void);

#define GodsonUpdateStallFetch(st)								\
  (st)->stall_fetch = (st)->stall_fpc || (st)->stall_branch ||	\
	(st)->stall_icache || (st)->stall_except ||					\
	(st)->stall_itlbmiss || (st)->stall_sys ||					\
	(st)->stall_redirect

#define IWIN_TLBFAULT   0x80000                 /* Op suffered a TLB fault */
#define IWIN_FAULT      0x1000                  /* Instruction faulted  */

#endif /* __GODSON2_CPU_H__ */
