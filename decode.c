/*
* decode.c - decode stage implementation
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
#include <math.h>
#include "mips.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "cache.h"
#include "bpred.h"
#include "tlb.h"
#include "eventq.h"
//#include "fetch.h"
//#include "decode.h"
//#include "issue.h"
//#include "writeback.h"
//#include "map.h"
//#include "commit.h"
#include "stats.h"
#include "syscall.h"
#include "sim.h"
#include "godson2_cpu.h"
#include "ptrace.h"
#include "sampling.h"

#include "istat.h"

/*This file implements the decodee stage of the pipeline. */

/* recover instruction trace generator state to precise state state immediately
   before the given LEVEL mis-predicted branch; this is accomplished by
   resetting all register value copied-on-write beyond LEVEL, and the
   speculative memory hash table above LEVEL is cleared */
void
tracer_recover(struct godson2_cpu *st,int level,md_addr_t recover_pc)
{
  int i;
  struct spec_mem_ent *ent, *ent_next, *head;

  /* better be in mis-speculative trace generation mode */
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
  /* could be exception */
  /*
    if (!spec_level)
      panic("cannot recover unless in speculative mode");
   */
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  /* reset to required trace generation mode */
  st->spec_level = level;

  /* reset memory state back to given level */
  for (i=0; i<STORE_HASH_SIZE; i++)
    {
      head = NULL;
      /* release all hash table buckets written in level LEVEL or deeper ones*/
      for (ent=st->store_htable[i]; ent; ent=ent_next)
	{
	  ent_next = ent->next;
	  if (ent->level >= level) {
	    ent->next = st->bucket_free_list;
	    st->bucket_free_list = ent;
	  }else {
	    /* reinsert */
	    ent->next = head;
	    head = ent;
	  }
	}
      st->store_htable[i] = head;
    }

  /* reset IFETCH state */

  /* free the instruction data */
  i = st->fetch_head;
  while (st->fetch_num>0) {
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
    /* indicate in pipetrace that this instruction was squashed */
    ptrace_endinst(st->fetch_data[i]->ptrace_seq);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
    fetch_return_to_free_list(st,st->fetch_data[i]);
    st->fetch_data[i]->tag++;
    st->fetch_num--;
    i = (i+1)%fetch_ifq_size;
  }
  st->fetch_tail = st->fetch_head = 0;
  st->fetch_reg_PC = recover_pc;

  /* cancel pending miss that hasnot been accepted */
  if (st->irepbuf.state == IREP_MISS) 
    st->irepbuf.state = IREP_EMPTY;
  
  /* reset decode state,do it in reverse order to recover committed values */
  i = (st->decode_tail + decode_ifq_size - 1) % decode_ifq_size;
  while (st->decode_num > 0) {
    int j;
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
    /* indicate in pipetrace that this instruction was squashed */
    ptrace_endinst(st->decode_data[i]->ptrace_seq);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

    /* recover any resources used by this roq operation */
    for (j=0; j<MAX_ODEPS; j++)
    {
      RSLINK_FREE_LIST(st->decode_data[i]->odep_list[j]);
      /* blow away the consuming op list */
      st->decode_data[i]->odep_list[j] = NULL;
    }

    recover_data(st,st->decode_data[i]);

    st->decode_data[i]->tag++;
    fetch_return_to_free_list(st,st->decode_data[i]);
    st->decode_num--;
    i = (i + decode_ifq_size - 1)%decode_ifq_size;
  }
  st->decode_tail = st->decode_head = 0;

  st->need_to_correct_pc = FALSE;
  st->entering_spec = FALSE;
  st->last_is_branch = FALSE;
  st->is_jump = 0;
}

/* initialize the speculative instruction state generator state */
static void
tracer_init(struct godson2_cpu *st)
{
  int i;

  /* initially in non-speculative mode */
  st->spec_level = 0;

  /* spec_level may increase to brq_ifq_size, although no instruction at level
   * brq_ifq_size will pass the map stage,but we may execute them in this stage
   * (thus use the spec_regs and cv),so we alloc one more 
   */
  st->spec_regs_R = calloc(sizeof(md_gpr_t)*(brq_ifq_size+1),1);
  st->spec_regs_F = calloc(sizeof(md_fpr_t)*(brq_ifq_size+1),1);
  st->spec_regs_C = calloc(sizeof(md_ctrl_t)*(brq_ifq_size+1),1);

  if (!st->spec_regs_R || !st->spec_regs_F || !st->spec_regs_C) {
    fatal("out of virtual memory\n");
  }

  /* memory state is from non-speculative memory pages */
  for (i=0; i<STORE_HASH_SIZE; i++)
    st->store_htable[i] = NULL;
}

/* initialize status for new level */
static void prepare_level(struct godson2_cpu *st,struct inst_descript *rs)
{
  /* for multi-level speculation, copy-on-write can't save too much time(I can't 
   * think of an efficient way to maintain NEWEST location for each level),so we
   * just copy the whole regfile */
  if (st->spec_level) {
    memcpy(&st->spec_regs_R[st->spec_level+1],&st->spec_regs_R[st->spec_level],sizeof(md_gpr_t));
    memcpy(&st->spec_regs_F[st->spec_level+1],&st->spec_regs_F[st->spec_level],sizeof(md_fpr_t));
    memcpy(&st->spec_regs_C[st->spec_level+1],&st->spec_regs_C[st->spec_level],sizeof(md_ctrl_t));
  }else{
    memcpy(&st->spec_regs_R[1],&st->regs.regs_R,sizeof(md_gpr_t));
    memcpy(&st->spec_regs_F[1],&st->regs.regs_F,sizeof(md_fpr_t));
    memcpy(&st->spec_regs_C[1],&st->regs.regs_C,sizeof(md_ctrl_t));
  }

  prepare_create_vector(st,st->spec_level);
}

/* speculative memory hash table address hash function */
#define HASH_ADDR(ADDR)							\
  ((((ADDR) >> 24)^((ADDR) >> 16)^((ADDR) >> 8)^(ADDR)) & (STORE_HASH_SIZE-1))

/* this functional provides a layer of mis-speculated state over the
   non-speculative memory state, when in mis-speculation trace generation mode,
   the simulator will call this function to access memory, instead of the
   non-speculative memory access interfaces defined in memory.h; when storage
   is written, an entry is allocated in the speculative memory hash table,
   future reads and writes while in mis-speculative trace generation mode will
   access this buffer instead of non-speculative memory state; when the trace
   generator transitions back to non-speculative trace generation mode,
   tracer_recover() clears this table, returns any access fault */
//static enum md_fault_type
static enum md_fault_type
spec_mem_access(struct godson2_cpu *st, /* cpu struct to access */
				struct mem_t *mem,		/* memory space to access */
                int cmd,		        /* Read or Write access cmd */
        		md_addr_t addr,			/* virtual address of access */
        		void *p,				/* input/output buffer */
	        	int nbytes)				/* number of bytes to access */
{
  int i, index;
  struct spec_mem_ent *ent = NULL, /**prev,*/ *newest = NULL;

  /* FIXME: partially overlapping writes are not combined... */
  /* FIXME: partially overlapping reads are not handled correctly... */

  /* check alignments, even speculative this test should always pass */
  if ((nbytes & (nbytes-1)) != 0 || (addr & (nbytes-1)) != 0)
    {
      /* no can do, return zero result */
      for (i=0; i < nbytes; i++)
	((char *)p)[i] = 0;

      return md_fault_none;
    }

  /* check permissions */
  if (!((addr >= st->ld_text_base && addr < (st->ld_text_base+st->ld_text_size)
	 && cmd == Read)
	|| MD_VALID_ADDR(st,addr)))
    {
      /* no can do, return zero result */
      for (i=0; i < nbytes; i++)
	((char *)p)[i] = 0;

      return md_fault_none;
    }

  /* has this memory state been copied on mis-speculative write? */
  index = HASH_ADDR(addr);

  /* for multi-level mis-speculation support,we add a version stamp--level to
   * each entry and search for the newest one. The cost is we have to scan the
   * whole list and reorder chains is useless.
   */
  for (/*prev=NULL,*/ent=st->store_htable[index]; ent; /*prev=ent,*/ent=ent->next)
    {
      int level = 0;
      if (ent->addr == addr && ent->level>level)
	{
	  level = ent->level;
	  newest = ent;

#if 0
	  /* reorder chains to speed access into hash table */
	  if (prev != NULL)
	    {
	      /* not at head of list, relink the hash table entry at front */
	      prev->next = ent->next;
              ent->next = store_htable[index];
              store_htable[index] = ent;
	    }
	  break;
#endif
	}
    }

  ent = newest;

  /* no, if it is a write, allocate a hash table entry to hold the data */
  if (!ent && cmd == Write)
    {
      /* try to get an entry from the free list, if available */
      if (!st->bucket_free_list)
	{
	  /* otherwise, call calloc() to get the needed storage */
	  st->bucket_free_list = calloc(1, sizeof(struct spec_mem_ent));
	  if (!st->bucket_free_list)
	    fatal("out of virtual memory");
	}
      ent = st->bucket_free_list;
      st->bucket_free_list = st->bucket_free_list->next;

      /* insert into hash table */
      ent->next = st->store_htable[index];
      st->store_htable[index] = ent;
      ent->addr = addr;
      ent->level = st->spec_level;
      ent->data[0] = 0; ent->data[1] = 0;
    }

  /* handle the read or write to speculative or non-speculative storage */
  switch (nbytes)
    {
    case 1:
      if (cmd == Read)
	{
	  if (ent)
	    {
	      /* read from mis-speculated state buffer */
	      *((byte_t *)p) = *((byte_t *)(&ent->data[0]));
	    }
	  else
	    {
	      /* read from non-speculative memory state, don't allocate
	         memory pages with speculative loads */
	      *((byte_t *)p) = MEM_READ_BYTE(st->mem,addr);
	    }
	}
      else
	{
	  /* always write into mis-speculated state buffer */
	  *((byte_t *)(&ent->data[0])) = *((byte_t *)p);
	}
      break;
    case 2:
      if (cmd == Read)
	{
	  if (ent)
	    {
	      /* read from mis-speculated state buffer */
	      *((half_t *)p) = *((half_t *)(&ent->data[0]));
	    }
	  else
	    {
	      /* read from non-speculative memory state, don't allocate
	         memory pages with speculative loads */
	      *((half_t *)p) = MEM_READ_HALF(st->mem, addr);
	    }
	}
      else
	{
	  /* always write into mis-speculated state buffer */
	  *((half_t *)&ent->data[0]) = *((half_t *)p);
	}
      break;
    case 4:
      if (cmd == Read)
	{
	  if (ent)
	    {
	      /* read from mis-speculated state buffer */
	      *((word_t *)p) = *((word_t *)&ent->data[0]);
	    }
	  else
	    {
	      /* read from non-speculative memory state, don't allocate
	         memory pages with speculative loads */
	      *((word_t *)p) = MEM_READ_WORD(st->mem, addr);
	    }
	}
      else
	{
	  /* always write into mis-speculated state buffer */
	  *((word_t *)&ent->data[0]) = *((word_t *)p);
	}
      break;
    case 8:
      if (cmd == Read)
	{
	  if (ent)
	    {
	      /* read from mis-speculated state buffer */
	      *((word_t *)p) = *((word_t *)&ent->data[0]);
	      *(((word_t *)p)+1) = *((word_t *)&ent->data[1]);
	    }
	  else
	    {
	      /* read from non-speculative memory state, don't allocate
	         memory pages with speculative loads */
	      *((word_t *)p) = MEM_READ_WORD(st->mem, addr);
	      *(((word_t *)p)+1) =
		MEM_READ_WORD(st->mem, addr + sizeof(word_t));
	    }
	}
      else
	{
	  /* always write into mis-speculated state buffer */
	  *((word_t *)&ent->data[0]) = *((word_t *)p);
	  *((word_t *)&ent->data[1]) = *(((word_t *)p)+1);
	}
      break;
    default:
      panic("access size not supported in mis-speculative mode");
    }

  return md_fault_none;
}


/*
 * configure the execution engine
 */

/* next program counter */
#define SET_NPC(EXPR)           (st->regs.regs_NPC = (EXPR))

/* target program counter */
#undef  SET_TPC
#define SET_TPC(EXPR)			(st->target_PC = (EXPR))

/* current program counter */
#define CPC                     (st->regs.regs_PC)
#define SET_CPC(EXPR)           (st->regs.regs_PC = (EXPR))

#if 1
#define SAVE_OLD(V,N,T) ( { current->temp_save[current->ti].n = (N); \
                          current->temp_save[current->ti].t = (T); \
                          if ((T)==0) current->temp_save[current->ti].l = (V); \
                          else if ((T)==1) { \
                             current->temp_save[current->ti].addr = (V); \
                             mem_access(st->mem,Read,(V),&current->temp_save[current->ti].l,sizeof(word_t)); \
                          } \
                          else if ((T)==2) current->temp_save[current->ti].f = (V); \
                          else if ((T)==3) current->temp_save[current->ti].d = (V); \
                          else current->temp_save[current->ti].l = (V); \
    			  /*if (sim_cycle>3740000) printf("pc %x v=%x\n",current->regs_PC,V);*/ \
			  current->ti++; } )
#else
/* would loss for float possible(arg pass) */
void SAVE_OLD(struct inst_descript *current,int V,int N,int T)
{
  current->temp_save[current->ti].n = (N); 
  current->temp_save[current->ti].t = (T);
  switch (T) {
    case 0:
      current->temp_save[current->ti].l = (V); 
      break;
    case 1:
      current->temp_save[current->ti].addr = (V); 
      mem_access(st->mem,Read,(V),&current->temp_save[current->ti].l,sizeof(word_t)); 
      break;
    case 2:
      current->temp_save[current->ti].f = (V); 
      break;
    case 3:
      current->temp_save[current->ti].d = (V); 
      break;
    default:
      current->temp_save[current->ti].l = (V); 
      break;
  }
  //printf("pc%x:%d\n",current->regs_PC,current->ti); 
  current->ti++; 
} 
#endif

/* general purpose register accessors, NOTE: speculative copy on write storage
   provided for fast recovery during wrong path execute (see tracer_recover()
   for details on this process */
#define GPR(N)            (st->spec_level			                     \
			  ? st->spec_regs_R[st->spec_level][(N)]                     \
			  : st->regs.regs_R[(N)])
#define SET_GPR(N,EXPR)   (st->spec_level				             \
			  ? (st->spec_regs_R[st->spec_level][(N)] = (EXPR))          \
		          : (SAVE_OLD(st->regs.regs_R[(N)],(N),0),st->regs.regs_R[(N)] = (EXPR)),current->regn=(N),current->regv=st->regs.regs_R[(N)])

/* floating point register accessors, NOTE: speculative copy on write storage
   provided for fast recovery during wrong path execute (see tracer_recover()
   for details on this process */
#define FPR_L(N)          (st->spec_level			                     \
			  ? st->spec_regs_F[st->spec_level].l[(N)]                   \
			  : st->regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)   (st->spec_level				             \
			  ? (st->spec_regs_F[st->spec_level].l[(N)] = (EXPR))        \
		          : (SAVE_OLD(st->regs.regs_F.f[(N)],(N),2),st->regs.regs_F.l[(N)] = (EXPR)))
#define FPR_F(N)          (st->spec_level			                     \
			  ? st->spec_regs_F[st->spec_level].f[(N)]                   \
			  : st->regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)   (st->spec_level				             \
			  ? (st->spec_regs_F[st->spec_level].f[(N)] = (EXPR))        \
		          : (SAVE_OLD(st->regs.regs_F.f[(N)],(N),2), st->regs.regs_F.f[(N)] = (EXPR)))
#define FPR_D(N)          (st->spec_level			                     \
			  ? st->spec_regs_F[st->spec_level].d[(N)>>1]                \
			  : st->regs.regs_F.d[(N)>>1])
#define SET_FPR_D(N,EXPR)   (st->spec_level				             \
			  ? (st->spec_regs_F[st->spec_level].d[(N)>>1] = (EXPR))     \
		          : (SAVE_OLD(st->regs.regs_F.d[(N)>>1],(N)>>1,3), st->regs.regs_F.d[(N)>>1] = (EXPR)))

/* miscellanous register accessors, NOTE: speculative copy on write storage
   provided for fast recovery during wrong path execute (see tracer_recover()
   for details on this process */
#define HI                (st->spec_level			                     \
			  ? st->spec_regs_C[st->spec_level].hi  	             \
			  : st->regs.regs_C.hi)
#define SET_HI(EXPR)      (st->spec_level				             \
			  ? (st->spec_regs_C[st->spec_level].hi = (EXPR))            \
		          : (SAVE_OLD(st->regs.regs_C.hi,0,4), st->regs.regs_C.hi = (EXPR)))
#define LO                (st->spec_level			                     \
			  ? st->spec_regs_C[st->spec_level].lo  	             \
			  : st->regs.regs_C.lo)
#define SET_LO(EXPR)      (st->spec_level				             \
			  ? (st->spec_regs_C[st->spec_level].lo = (EXPR))            \
		          : (SAVE_OLD(st->regs.regs_C.lo,0,5), st->regs.regs_C.lo = (EXPR)))
#define FCC               (st->spec_level			                     \
			  ? st->spec_regs_C[st->spec_level].fcc  	             \
			  : st->regs.regs_C.fcc)
#define SET_FCC(EXPR)     (st->spec_level				             \
			  ? (st->spec_regs_C[st->spec_level].fcc = (EXPR))           \
		          : (SAVE_OLD(st->regs.regs_C.fcc,0,6), st->regs.regs_C.fcc = (EXPR)))

/* precise architected memory state accessor macros, NOTE: speculative copy on
   write storage provided for fast recovery during wrong path execute (see
   tracer_recover() for details on this process */
#define __READ_SPECMEM(SRC, SRC_V, FAULT)				\
  (addr = (SRC), 							\
   (st->spec_level								\
    ? ((FAULT) = spec_mem_access(st, st->mem, Read, addr, &SRC_V, sizeof(SRC_V)))\
    : ((FAULT) = mem_access(st->mem, Read, addr, &SRC_V, sizeof(SRC_V)))),	\
   SRC_V)

#define READ_BYTE(SRC, FAULT)						\
  __READ_SPECMEM((SRC), temp_byte, (FAULT))
#define READ_HALF(SRC, FAULT)						\
  MD_SWAPH(__READ_SPECMEM((SRC), temp_half, (FAULT)))
#define READ_WORD(SRC, FAULT)						\
  MD_SWAPW(__READ_SPECMEM((SRC), temp_word, (FAULT)))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  MD_SWAPQ(__READ_SPECMEM((SRC), temp_qword, (FAULT)))
#endif /* HOST_HAS_QWORD */


#define __WRITE_SPECMEM(SRC, DST, DST_V, FAULT)				\
  (DST_V = (SRC), addr = (DST),						\
   (st->spec_level \
    ? ((FAULT) = spec_mem_access(st, st->mem, Write, addr, &DST_V, sizeof(DST_V)))\
    : (SAVE_OLD((addr&~0x3),0,1),(FAULT) = mem_access(st->mem, Write, addr, &DST_V, sizeof(DST_V)))))

#define WRITE_BYTE(SRC, DST, FAULT)					\
  __WRITE_SPECMEM((SRC), (DST), temp_byte, (FAULT))
#define WRITE_HALF(SRC, DST, FAULT)					\
  __WRITE_SPECMEM(MD_SWAPH(SRC), (DST), temp_half, (FAULT))
#define WRITE_WORD(SRC, DST, FAULT)					\
  __WRITE_SPECMEM(MD_SWAPW(SRC), (DST), temp_word, (FAULT))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  __WRITE_SPECMEM(MD_SWAPQ(SRC), (DST), temp_qword, (FAULT))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST)							\
  (/* only execute system calls in non-speculative mode */		\
   (st->spec_level ? panic("speculative syscall") : (void) 0),		\
   sys_syscall(st, &st->regs, mem_access, st->mem, INST, TRUE))


/*
 * the create vector maps a logical register to a creator in the roq (and
 * specific output operand) or the architected register file (if RS_link
 * is NULL)
 */

/* a NULL create vector entry */
static struct CV_link CVLINK_NULL = { NULL, 0 };

/* get a new create vector link */
#define CVLINK_INIT(CV, RS,ONUM)	((CV).rs = (RS), (CV).odep_num = (ONUM))

/* read a create vector entry */
#define CREATE_VECTOR(st,N)        (st->create_vector[st->spec_level][(N)])

/* read a create vector timestamp entry */
#define CREATE_VECTOR_RT(st,N)     (st->create_vector_rt[st->spec_level][(N)] )

/* set a create vector entry */
#define SET_CREATE_VECTOR(st,N,L)  (st->create_vector[st->spec_level][(N)] = (L))

/* initialize the create vector */
static void
cv_init(struct godson2_cpu *st)
{
  int i,j;

  /* spec_level may increase to brq_ifq_size, although no instruction at level
   * brq_ifq_size will pass the map stage,but we may execute them in this stage
   * (thus use the spec_regs and cv),so we alloc one more 
   */
  st->create_vector = (struct CV_link**)malloc(sizeof(struct CV_link*) * (brq_ifq_size+1));
  st->create_vector_rt = (tick_t **)malloc(sizeof(tick_t *) * (brq_ifq_size+1));
  for (i=0; i <= brq_ifq_size; i++) {
    st->create_vector[i] = (struct CV_link*)malloc(sizeof(struct CV_link) * MD_TOTAL_REGS);
    st->create_vector_rt[i] = (tick_t *)malloc(sizeof(tick_t) * MD_TOTAL_REGS);
  }
  /* initially all registers are valid in the architected register file,
     i.e., the create vector entry is CVLINK_NULL */
  for (i=0; i <= brq_ifq_size; i++)
    for (j=0; j < MD_TOTAL_REGS; j++)
    {
      st->create_vector[i][j] = CVLINK_NULL;
      st->create_vector_rt[i][j] = 0;
    }
}

/* for exception flush */
void clear_create_vector(struct godson2_cpu *st/*struct inst_descript *rs*/)
{

  memset(st->create_vector[0],0,sizeof(struct CV_link)*MD_TOTAL_REGS);
  /*
  struct CV_link head;
  head = CREATE_VECTOR(rs->out1);
  if (head.rs == rs) {
    create_vector[0][rs->out1] = CVLINK_NULL;
  }
  */
}

/* link RS onto the output chain number of whichever operation will next
   create the architected register value IDEP_NAME */
static INLINE void
roq_link_idep(struct godson2_cpu *st, 
	      struct inst_descript *rs,		/* rs station to link */
	      int idep_num,			/* input dependence number */
	      int idep_name)			/* input register name */
{
  struct RS_link *link;
  struct CV_link head;

  /* any dependence? */
  if (idep_name == NA)
    {
      /* no input dependence for this input slot, mark operand as ready */
      rs->idep_ready[idep_num] = TRUE;
      return;
    }

  /* locate creator of operand */
  head = CREATE_VECTOR(st,idep_name);

  /* any creator? */
  if (!head.rs)
    {
      /* no active creator, use value available in architected reg file,
         indicate the operand is ready for use */
      rs->idep_ready[idep_num] = TRUE;

      return;
    }
  /* else, creator operation will make this value sometime in the future */

  /* indicate value will be created sometime in the future, i.e., operand
     is not yet ready for use */
  rs->idep_ready[idep_num] = FALSE;

  /* link onto creator's output list of dependant operand */
  RSLINK_NEW(link, rs);  /*this macro need st as argument*/
  link->x.opnum = idep_num;
  link->next = head.rs->odep_list[head.odep_num];
  head.rs->odep_list[head.odep_num] = link;

  //myfprintf(stderr,"inst %x(%d) depends inst %x(%d)\n",rs->regs_PC,idep_num,head.rs->regs_PC,head.odep_num);
}

/* make RS the creator of architected register ODEP_NAME */
static INLINE void
roq_install_odep(struct godson2_cpu *st,
	     struct inst_descript *rs,	/* creating RUU station */
		 int odep_num,			/* output operand number */
		 int odep_name)			/* output register name */
{
  struct CV_link cv;

  /* any dependence? */
  if (odep_name == NA)
    {
      /* no value created */
      rs->onames[odep_num] = NA;
      rs->odep_list[odep_num] = NULL;
      return;
    }
  /* else, create a RS_NULL terminated output chain in create vector */

  /* record output name, used to update create vector at completion */
  rs->onames[odep_num] = odep_name;

  /* initialize output chain to empty list */
  rs->odep_list[odep_num] = NULL;

  /* indicate this operation is latest creator of ODEP_NAME */
  CVLINK_INIT(cv, rs, odep_num);
  SET_CREATE_VECTOR(st, odep_name, cv);
}

void prepare_create_vector(struct godson2_cpu *st,int level)
{
  memcpy(st->create_vector[level+1],st->create_vector[level],sizeof(struct CV_link)*MD_TOTAL_REGS);
}

void update_create_vector(struct godson2_cpu *st,struct inst_descript *rs)
{
  struct CV_link link;
  int i,level;
  for (i=0; i<MAX_ODEPS; i++)
  {
    if (rs->onames[i] != NA)
    {
      level = rs->spec_level;
      do {
    	link = st->create_vector[level][rs->onames[i]];
    	if (/* !NULL */link.rs
    	    && /* refs RS */(link.rs == rs && link.odep_num == i))
    	{
	  /* the result can now be read from a physical register,
	     indicate this as so */
    	  st->create_vector[level][rs->onames[i]] = CVLINK_NULL;
	  //create_vector_rt[rs->onames[i]] = sim_cycle;
    	}else
    	  break;
	/* else, creator invalidated or there is another creator */

	/* this vector might have been copied to higher level,clear them
	 * if not overrided 
	 */
    	level++;
      } while ( level <= st->spec_level );
    }
  }
}

static inline int leading_zero(unsigned int a)
{
  int i;
  for (i=0;i<32;i++) {
    if (a & (1<<(31-i))) return i;
  }
  return 32;
}

static inline int calculate_div_delay(int opa,int opb)
{
  int res = 0;
  unsigned int a = (unsigned int)opa;
  unsigned int b = (unsigned int)opb;
  int azero,bzero,czero;

  azero = leading_zero(a); /* leading zero of opa */
  bzero = leading_zero(b); /* leading zero of opb */
  czero = ((b!=0&&a%b==0)?ffs(a/b)-1:0); /* zero at the end of the result if remainder==0 */

  if (bzero > azero) {
    res =  (bzero - azero)/2 + 4 - czero/2;
  }
  return(res>0 ? res : 1);
}


void decode_stage_init(struct godson2_cpu *st)
{
  st->decode_data = (struct inst_descript **)
    calloc(decode_ifq_size, sizeof(struct inst_descript*));
  
  if (!st->decode_data) 
    fatal ("out of virtual memory");
  
  st->decode_num = 0;
  st->decode_head = 0;
  st->decode_tail = 0;

  tracer_init(st);

  cv_init(st);
}

#if 0

void decode_redirect_fetch(tick_t now,int arg)
{
  int i;

  i = fetch_head;
  while (fetch_num>0) {
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
    /* squash the next instruction from the fetch queue */
    ptrace_endinst(fetch_data[i]->ptrace_seq);
#endif
    fetch_return_to_free_list(fetch_data[i]);
    fetch_num--;
    i = (i+1)%fetch_ifq_size;
  }
  fetch_head = 0;
  fetch_tail = 0;

  need_to_correct_pc = 0;

  fetch_reg_PC = correct_PC;
  fetch_istall_buf.stall = 0;

  //myfprintf(stderr,"decoder redirect pc to %x\n",correct_PC);
}

#endif

void decode_stage(struct godson2_cpu *st)
{
  int i;
  md_inst_t inst;
  int op;
  md_addr_t pred_PC;
  struct bpred_update_t *dir_update_ptr;
  int stack_recover_idx;
  int branch_cnt = 0;
  int same_cycle = FALSE;
  int br_pred_taken = FALSE;
  int br_taken = FALSE;
  md_addr_t addr;
  int is_write = FALSE; /* for dlite */
  enum md_fault_type fault;
  byte_t temp_byte = 0;			/* temp variable for spec mem access */
  half_t temp_half = 0;			/* " ditto " */
  word_t temp_word = 0;			/* " ditto " */
# ifdef HOST_HAS_QWORD
  //qword_t temp_qword = 0;		/* " ditto " */
# endif /* HOST_HAS_QWORD */
  int in1,in2,in3,out1,out2;

  /* update ghr for last cycle, do it here can help to ensure if brbus seen
   * it will be cancelled. Use an event is difficult to ensure this.
   */
  if (st->ghr_valid) {
    decode_update_ghr(st->pred,st->ghr_predict);
    st->ghr_valid = FALSE;
  }
  st->ghr_predict = 0;

  /* map stage can't absort output? */
  if (st->decode_num + decode_width > decode_ifq_size) return;

  while (/* has instruction to decode */
         st->fetch_num!=0 &&
         /* has space to queue output */
     	 st->decode_num < decode_ifq_size &&
    	 /* branch instruction count is less than decode_speed */
    	 branch_cnt < decode_speed 
        )
  {
      struct inst_descript *current = st->fetch_data[st->fetch_head];

      op = current->op;

      /* access counter added for our power model */
      st->decode_access++;

      if (st->last_is_branch) {
    	/* what if continous branch seen? */
    	if (MD_OP_FLAGS(op) & F_CTRL) {
    	  if (st->spec_level==0) fatal("continous branch %x\n",current->regs_PC);
    	  else {
			warn("continous branch %x\n",current->regs_PC);
    	    current->IR = MD_NOP_INST;
     	    /* Set the opcode */
    	    MD_SET_OPCODE(current->op,current->IR);
    	    op = current->op;
    	  }
    	}
    	current->bd = TRUE;
    	st->last_is_branch = FALSE;
    	branch_cnt ++;
      }

      /* get the next instruction from the fetch -> decode queue */
      inst              = current->IR;
      st->regs.regs_PC  = current->regs_PC;
      pred_PC           = current->pred_PC;
      dir_update_ptr    = &current->dir_update;

      /* compute default next PC */
      st->regs.regs_NPC = st->regs.regs_PC + sizeof(md_inst_t);

      /* drain RUU for TRAPs and system calls */
      if (MD_OP_FLAGS(op) & F_TRAP){
		if (st->roq_num != 0)  break;

	  /* else, syscall is only instruction in the machine, at this
	     point we should not be in (mis-)speculative mode */
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
	    if (st->spec_level)
	      panic("drained and speculative");
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
   	  }

      if (st->is_jump) {
    	/* correct NPC for taken delayslot for mode switch */
     	current->regs_NPC = st->target_PC;
      }
      st->is_jump = 0;

      /* maintain $r0 semantics (in spec and non-spec space) */
      st->regs.regs_R[MD_REG_ZERO] = 0; 
      /* is it necessary ? */
      //spec_regs_R[MD_REG_ZERO] = 0;

      /* default effective address (none) and access */
      addr = 0;   is_write = FALSE;

      current->ti = 0;
      current->regn = -1;

      /* set default fault - none */
      fault = md_fault_none;

      /* more decoding and execution */
      switch (op)
	{
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,O2,I1,I2,I3)		\
	case OP:							\
	  /* compute output/input dependencies to out1-2 and in1-3 */	\
	  out1 = O1; out2 = O2;						\
	  in1 = I1; in2 = I2; in3 = I3;					\
	  /* execute the instruction */					\
	  SYMCAT(OP,_IMPL);						\
	  break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
	case OP:							\
	  /* could speculatively decode a bogus inst, convert to NOP */	\
	  op = MD_NOP_OP;						\
	  /* compute output/input dependencies to out1-2 and in1-3 */	\
	  out1 = NA; out2 = NA;						\
	  in1 = NA; in2 = NA; in3 = NA;					\
	  /* no EXPR */							\
	  break;
#define CONNECT(OP)	/* nada... */
	  /* the following macro wraps the instruction fault declaration macro
	     with a test to see if the trace generator is in non-speculative
	     mode, if so the instruction fault is declared, otherwise, the
	     error is shunted because instruction faults need to be masked on
	     the mis-speculated instruction paths */
#define DECLARE_FAULT(FAULT)						\
	  {								\
	    if (!st->spec_level)						\
	      fault = (FAULT);						\
	    /* else, spec fault, ignore it, always terminate exec... */	\
	    break;							\
	  }
#include "mips.def"
	default:
	  /* can speculatively decode a bogus inst, convert to a NOP */
	  op = MD_NOP_OP;
	  /* compute output/input dependencies to out1-2 and in1-3 */	\
	  out1 = NA; out2 = NA;
	  in1 = NA; in2 = NA; in3 = NA;
	  /* no EXPR */
	}

      if (op==DIVU || op==SPLITDIV || op==DIV) {
    	current->div_delay = calculate_div_delay(GPR(RS),GPR(RT));
      }

#if 0
      if (spec_level==0 && current->regs_PC == 0x47d25c) {
	printf("%lld,is_jump=%d,rs=%d(%x),inst=%x,gpr=%x\n",sim_cycle,is_jump,RS,regs.regs_R[RS],inst,GPR(RS));
      }
      if (spec_level==0 && current->regs_PC >= 0x47d244 && current->regs_PC < 0x47d25c) {
	printf("regn=%d,regv=%x,regs.regs_R=%x\n",current->regn,current->regv,regs.regs_R[6]);
	if (current->regn!=-1) printf("regn=%x\n",regs.regs_R[current->regn]);
      }
#endif

#if 0
      if (!spec_level) {
	if (op != SPLITMUL && op != SPLITDIV) {
	  sim_pop_insn++;
	}
      }

      if (!spec_level && verbose && (sim_pop_insn % opt_inst_interval == 0)) 
      {
	if (op != SPLITMUL && op != SPLITDIV) {
	  myfprintf(stdout, "%10n @ 0x%08p: ", sim_pop_insn, current->regs_PC);
	  md_print_insn(current->IR, current->regs_PC, stdout);
	  fprintf(stdout, "\n");
	}
	/* fflush(stderr); */
      }
#endif

      if (current->ti>=3) {
    	fatal("too many arch changes at %x\n",current->regs_PC);
      }

      if (fault != md_fault_none) 
    	fatal("non-speculative fault (%d) detected @ 0x%08p,ra=%x", fault, st->regs.regs_PC, st->regs.regs_R[31]);

      /* mips target does not set NPC for instructions */
      if (st->is_jump) {
    	st->regs.regs_NPC = st->target_PC;
      }

      /* register mapping can be done here in fact,but to keep logic clear, we
       * leave it to the map stage
       */
      current->in1  = in1;
      current->in2  = in2;
      current->in3  = in3;
      current->out1 = out1;
      current->out2 = out2;

      /* generate dep lists */
      /* link onto producing operation */
      roq_link_idep(st, current, /* idep_ready[] index */0, in1);
      roq_link_idep(st, current, /* idep_ready[] index */1, in2);
      roq_link_idep(st, current, /* idep_ready[] index */2, in3);

      /* install output after inputs to prevent self reference */
      roq_install_odep(st, current, /* odep_list[] index */0, out1);
      roq_install_odep(st, current, /* odep_list[] index */1, out2);


      /* one more instruction executed, speculative or otherwise */
      st->sim_total_insn++;
      if (MD_OP_FLAGS(op) & F_CTRL)
    	st->sim_total_branches++;

#if 0
      if (!spec_level)
      {
	/* one more non-speculative instruction executed */
	if (simulator_state == MEASURING) {
	  sim_meas_insn++;
	}
	sim_detail_insn++;
	current->sim_pop_insn = sim_pop_insn++;
        if (MD_OP_FLAGS(op) & F_CTRL) {
	  sim_num_branches++;
	}

	/* print retirement trace if in verbose mode */
	if (verbose && (sim_pop_insn % opt_inst_interval == 0)) 
	{
	  myfprintf(stderr, "%10n @ 0x%08p: ", sim_pop_insn, current->regs_PC);
 	  md_print_insn(current->IR, current->regs_PC, stderr);
	  fprintf(stderr, "\n");
	  /* fflush(stderr); */
	}

    if (sim_pop_insn>=25000000 && sim_pop_insn <=25010000) {
	  myfprintf(stderr, "-%10n @ 0x%08p: ", sim_pop_insn, current->regs_PC);
 	  md_print_insn(current->IR, current->regs_PC, stderr);
	  fprintf(stderr, "\n");
    }

      }
#endif

	  if (!st->spec_level)	
        st->last_non_spec_rs = current;

      /* update memory access stats */
      if (MD_OP_FLAGS(op) & F_MEM)
      {
    	st->sim_total_refs++;
    	if (!st->spec_level)
    	  st->sim_num_refs++;

    	if (MD_OP_FLAGS(op) & F_STORE)
    	  is_write = TRUE;
    	else
    	{
    	  st->sim_total_loads++;
    	  if (!st->spec_level)
    	    st->sim_num_loads++;
    	}
      }


      if (MD_OP_FLAGS(op) & F_CTRL) {

    	/* fix predition direction */
    	if (MD_OP_FLAGS(op) & (F_UNCOND | F_CTRL_LIKELY)) {
    	  /* change prediction for unconditional/likely branch */
    	  current->pred_taken = 1;
    	}
    	/* don't miss at this time to help draining */
    	if (st->simulator_state == DRAINING || pred_perfect) {
    	  current->pred_taken = st->is_jump;
    	}
    	br_pred_taken = current->pred_taken;
    	/* real direction */
    	br_taken = st->is_jump;
    	current->br_taken = br_taken;

    	/* fix prediction pc */
    	if (st->simulator_state == DRAINING || (MD_OP_FLAGS(op) & F_DIRJMP) || pred_perfect) {
    	  if (br_pred_taken) {
    	    pred_PC = st->target_PC;
    	  }else {
    	    pred_PC = st->regs.regs_PC + sizeof(md_inst_t);
    	  }
    	} else {
    	  /* indirect jumps */
    	  if ( MD_IS_RETURN(op) && st->pred->retstack.size) {
    	    pred_PC = st->pred->retstack.stack[st->pred->retstack.tos].target;
    	    st->pred->retstack.tos = (st->pred->retstack.tos + st->pred->retstack.size - 1) % st->pred->retstack.size;
            st->pred->retstack_pops++;
            dir_update_ptr->dir.ras = TRUE; /* using RAS here */
    	    /* twenisch - This fixes a bug in the RAS recovery when a 
     	       speculative return is squashed */
    	  } else {
    	  /* others should use the btb output in fetch stage */
    	  /* bug: use fetch_reg_PC */
    	    //pred_PC = fetch_reg_PC;
    	  }
//#define GODSON2B
#ifdef GODSON2B
	  /* no btb & ras */
    	  pred_PC = st->fetch_reg_PC;
#endif
    	}

    	/* ras push */
    	if (!pred_perfect && MD_IS_CALL(op)) {
     	  st->pred->retstack.tos = (st->pred->retstack.tos + 1) % st->pred->retstack.size;
    	  /* twenisch - This fixes a bug in the RAS recovery when a 
    	     speculative return is squashed */
    	  stack_recover_idx = st->pred->retstack.tos;
    	  st->pred->retstack.stack[st->pred->retstack.tos].target =
    	    st->regs.regs_PC + 2 * sizeof(md_inst_t);
    	  st->pred->retstack_pushes++;
    	}

    	stack_recover_idx = st->pred->retstack.tos;
    	current->stack_recover_idx = stack_recover_idx;
    	current->pred_PC = pred_PC;

     	if (current->bht_op && st->pred->class==BPred2Level) {
    	  st->ghr_valid = TRUE;
    	  st->ghr_predict = br_pred_taken;
    	}

    	//printf("pc:%x bp addr %x,v %d\n",current->regs_PC,dir_update_ptr->pdir1,*dir_update_ptr->pdir1);

    	st->last_is_branch = TRUE;

    	if (st->is_jump) 
    	  current->btarget = st->target_PC;
    	else
    	  current->btarget = st->regs.regs_NPC;

	  }

      /* remember current spec level */
      current->spec_level = st->spec_level;

      /* we must be in a delay slot,and the preceeding branch has wrong but
       * correctable PC
       */
      if (st->need_to_correct_pc) {
    	/* If the previous predicated taken branch is not the last instruction,
    	 * that is,its delayslot is fetched on the same cycle with it, clear
    	 * any remaining instructions,then schedule an event to cancel next
    	 * input and change the pc(if we change pc immediately,current cycle
    	 * fetch will use it, this is inconsistent with the hardware). 
    	 *
    	 * If it is the delay slot instruction fetched in different cycle
    	 * with the branch. the fetch PC should be changed immediately,because
    	 * the hardware has changed the fetch pc when this instruction is being
    	 * fetched,current cycle fetch can see new pc.
    	 *
    	 * In any case,the fetch queue is reset.
    	 */
    	if (same_cycle) {
#if 0
	  /* Queue an event to cancel next fetch and set new pc,then
	   * resume the fetch engine 
	   */
	  eventq_queue_callback(sim_cycle + 1, 
	                        (void *) decode_redirect_fetch,
	                        0);
#else
   	/* current cycle fetch should be stalled, new pc is meant for next cycle */
       	  st->fetch_istall_buf.stall |= BRANCH_STALL;
     	  st->fetch_reg_PC = st->correct_PC;
#endif
    	}else{
    	  st->fetch_reg_PC = st->correct_PC;
    	}
    	{
    	  /* reclaim inst template for remaining insts */
    	  int index = (st->fetch_head + 1) % fetch_ifq_size;
    	  while (st->fetch_num > 1) {
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
    	    /* squash the next instruction from the fetch queue */
    	    ptrace_endinst(st->fetch_data[index]->ptrace_seq);
#endif
    	    fetch_return_to_free_list(st,st->fetch_data[index]);
     	    index = (index + 1) % fetch_ifq_size;
    	    st->fetch_num--;
    	  }
    	}
    	/* will be decrease at the end of this function */
    	st->fetch_head = (fetch_ifq_size-1);
    	st->fetch_num = 1;
    	st->fetch_tail = 0;

    	st->need_to_correct_pc = 0;
      }else if ((pred_PC != st->regs.regs_NPC && pred_perfect)
    	  || ((MD_OP_FLAGS(op) & F_CTRL) == F_CTRL && br_pred_taken)) {
	/* Either 1) we're simulating perfect prediction and are in a
	   mis-predict state and need to patch up, or 2) We're not simulating
	   perfect prediction, we've predicted the branch taken, then we will
	   have to correct the pc since godson2 doesn't predict pc at fetch,
	 */
    	st->need_to_correct_pc = 1;
	/* was: if (pred_perfect) */
    	if (pred_perfect) {
    	  st->correct_PC = br_taken ? st->target_PC : st->regs.regs_PC + 2 *sizeof(md_inst_t);
    	  pred_PC = st->regs.regs_NPC;
    	}else {
	  /* notice that original ss set correct_PC to the REAL correct pc:
	   * regs.regs_NPC,so it will not enter spec_mode if fetch_redirected
	   * For us,need_to_correct_pc doesn't affect entering_spec
	   */
    	  st->correct_PC = pred_PC;
    	}
    	same_cycle = TRUE;
      }

      /* for cancelling */
      current->spec_level = st->spec_level;

      /* for load/store */
      current->addr = addr;

      current->mis_predict = FALSE;
      current->recover_inst = FALSE;
      /* is it transitioning into another level of mis-speculation mode? */
	  if (st->entering_spec) {
		prepare_level(st,current);
		st->spec_level++;
		/* not used */
		current->recover_inst = TRUE;
		current->recover_PC   = st->recover_PC;
		st->entering_spec  = FALSE;
	  }else if ((MD_OP_FLAGS(op) & F_CTRL) && 
		  pred_PC != st->regs.regs_NPC) {
	    /*
	        if (current->jr_op) 
		  printf("pred %x for jr at %x,real=%x\n",pred_PC,current->regs_PC,regs.regs_NPC);
		  */
		current->mis_predict = TRUE;
		/* set early, recover_PC is not used at this time */
		st->recover_PC = current->recover_PC = br_taken ? st->target_PC : st->regs.regs_PC + 2 *sizeof(md_inst_t);

		/* if likely branch is predicated wrong, then it must be not taken,
		 * so we have to cancel the delay slot
		 */
		if (MD_OP_FLAGS(op) & F_CTRL_LIKELY) {
		  prepare_level(st,current);
		  st->spec_level++;
		} else {
		  st->entering_spec = TRUE;
		}
	  }


#ifdef ISTAT
      current->decode_stamp = sim_cycle;
#endif

      /* consume instruction from fetch -> decode queue */
      st->decode_data[st->decode_tail] = current;
      st->decode_num++;
      st->decode_tail = (st->decode_tail + 1) & (decode_ifq_size - 1);

      st->fetch_head = (st->fetch_head + 1) & (fetch_ifq_size - 1);
      st->fetch_num--;

# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
      ptrace_newstage(current->ptrace_seq, PST_DECODE, 0);
      /* update any stats tracked by PC */

      for (i=0; i<pcstat_nelt; i++) {
        counter_t newval;
        int delta;

        /* check if any tracked stats changed */
        newval = STATVAL(pcstat_stats[i]);
        delta = newval - pcstat_lastvals[i];
        if (delta != 0) {
          stat_add_samples(pcstat_sdists[i], regs.regs_PC, delta);
    	  pcstat_lastvals[i] = newval;
        }
      }
#endif

      st->sim_decode_insn ++;
  }

#if 0
  if (ghr_valid) {
    /* Queue an event to update ghr */
    eventq_queue_callback(sim_cycle + 1, (void *) decode_update_ghr,
	ghr_predict);
  }
#endif
}


