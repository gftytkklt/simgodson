/*
 * sampling.c - SMARTS simulation support 
 *
 * This file is written by godson cpu group.
 * It has been written by extending the alphasim tool suite written by
 * Raj Desikan,which is in turn based on simplescalar.
 *  
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
 * Copyright (C) 1999 by Raj Desikan
 *
 * Copyright (C) 2005 by Fuxin Zhang
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

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#undef NO_INSN_COUNT

#include "host.h"
#include "misc.h"
#include "mips.h"
#include "options.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "eventq.h"
#include "syscall.h"
#include "stats.h"
#include "sim.h"
#include "issue.h"
#include "writeback.h"
#include "cache.h"
#include "resource.h"
#include "commit.h"
#include "bus.h"
#include "tlb.h"
#include "dram.h"
#include "bpred.h"
#include "fetch.h"
#include "decode.h"
#include "map.h"
#include "lsq.h"
#include "cache2mem.h"

#include "power.h"
#include "sampling.h"

#include "ptrace.h"

/* simulator state */
enum simulator_state_t simulator_state = NOT_STARTED;

/* sampling enabled.  1 if enabled, 0 otherwise */
unsigned int sampling_enabled;

/* sampling k value */
int sampling_k;

/* sampling measurement unit */
int sampling_munit;

/* sampling measurement unit */
int sampling_wunit;

/* TRUE if in-order warming is enabled*/
int sampling_allwarm;

/* sample size*/
counter_t sim_sample_size = 0;

/* period of each sample */
counter_t sim_sample_period = 0;

/* total number of instructions measured */
counter_t sim_detail_insn = 0;

/* total number of instructions measured */
counter_t sim_meas_insn = 0;

/* measured cycle */
tick_t sim_meas_cycle = 0;

/* Sampling standard deviation variables */
struct stddev_entry_t {
    int cycles[1024];
    struct stddev_entry_t* next;
};
struct stddev_entry_t* stddev_head = NULL;
struct stddev_entry_t* stddev_current = NULL;
int stddev_index = 0;

double standardDeviation = 0;
double percentageErrorInterval = 0;  /* not multiplied by 100%, so 3% is 0.03 */
int recommendedK = 0;  /* K recommended to achieve +/- 3%*/

#if 0
  struct pwr_stddev_entry_t {
      double pwr[1024];
      struct pwr_stddev_entry_t* next;
  };
  struct pwr_stddev_entry_t* pwr_stddev_head = NULL;
  struct pwr_stddev_entry_t* pwr_stddev_current = NULL;
  int pwr_stddev_index = 0;
 
  static double pwr_standardDeviation = 0;
  static double pwr_percentageErrorInterval = 0;  /* not multiplied by 100%, so 3% is 0.03 */
  static int pwr_recommendedK = 0;  /* K recommended to achieve +/- 3%*/
  static counter_t pwr_sample_size = 0;

  /* call once per sample to update running total for stdev calculation */
  void pwr_newSample(const double power) {
      if (pwr_stddev_current == NULL) {
         pwr_stddev_current = pwr_stddev_head = calloc(1, sizeof(struct pwr_stddev_entry_t));
         if (!pwr_stddev_head) fatal("out of virtual memory");
         pwr_stddev_head->next = NULL;
         pwr_stddev_index = 0;
      } else if (pwr_stddev_index == 1024) {
         pwr_stddev_current->next = calloc(1, sizeof(struct pwr_stddev_entry_t));
         if (!pwr_stddev_current) fatal("out of virtual memory");
         pwr_stddev_current = pwr_stddev_current->next;
         pwr_stddev_current->next = NULL;
         pwr_stddev_index = 0;
      }

      pwr_stddev_current->pwr[pwr_stddev_index] = power;
      ++pwr_stddev_index;
  
      ++pwr_sample_size;
  }
#endif

/* call once per sample to update running total for stdev calculation */
static void newSample(const int cycles) 
{
    if (stddev_current == NULL) {
       stddev_current = stddev_head = calloc(1, sizeof(struct stddev_entry_t));
       if (!stddev_head) fatal("out of virtual memory");
       stddev_head->next = NULL;
       stddev_index = 0;
    } else if (stddev_index == 1024) {
       stddev_current->next = calloc(1, sizeof(struct stddev_entry_t));
       if (!stddev_current) fatal("out of virtual memory");
       stddev_current = stddev_current->next;
       stddev_current->next = NULL;
       stddev_index = 0;
    }

    stddev_current->cycles[stddev_index] = cycles;
    ++stddev_index;

    ++sim_sample_size;
}

static const double to_percent = 100.0;
static const double z_score = 3.0;  //Corresponds to 99.7% confidence
static const double desired_error = 0.03; //+/- 3% error 

  //void pwr_doStatistics();

/* updates standardDeviation */
void doStatistics(void) 
{
  double sum = 0.0;
  double variance = 0.0;
  double mean_cycles = ((double) sim_meas_cycle) / sim_sample_size;
  struct stddev_entry_t* stddev_iter = stddev_head;
  int index = 0;
  double n_tuned = 0.0;

  if (!sampling_enabled) {
    standardDeviation = 0.0;
    percentageErrorInterval = 0.0;
    recommendedK = 0;
    return; 
  }

  if (!stddev_head)
    return; /* Avoid possible divisions by zero */

  while ( stddev_iter->next != NULL || index < stddev_index) {
    double x = stddev_iter->cycles[index++] - mean_cycles;
    sum += x * x;
    if (index == 1024) {
      stddev_iter = stddev_iter->next;
      index = 0;
    }
  }
  variance = sum / (sim_sample_size - 1);

  standardDeviation = sqrt(variance);
  //3.0 corresponds to the Z score for 99.7% confidence
  percentageErrorInterval =  z_score * ( standardDeviation ) / sqrt( sim_sample_size ) / (mean_cycles ) * to_percent ;
  n_tuned =  ( z_score * standardDeviation / mean_cycles / desired_error ) * ( z_score * standardDeviation / mean_cycles / desired_error );
  recommendedK = sim_pop_insn / sampling_munit / n_tuned; 

    //pwr_doStatistics();
}

#if 0
  extern double rename_power_cc3,bpred_power_cc3,lsq_power_cc3,window_power_cc3,regfile_power_cc3,icache_power_cc3,resultbus_power_cc3,clock_power_cc3, alu_power_cc3, dcache_power_cc3, dcache2_power_cc3;

  /* updates standardDeviation */
  void pwr_doStatistics() {
  	double sum = 0.0;
  	double variance = 0.0;
  	double mean_power = (rename_power_cc3 + bpred_power_cc3 + lsq_power_cc3 + window_power_cc3 + regfile_power_cc3 + icache_power_cc3 + resultbus_power_cc3 + clock_power_cc3 + alu_power_cc3 + dcache_power_cc3 + dcache2_power_cc3) / pwr_sample_size;
  	struct pwr_stddev_entry_t* stddev_iter = pwr_stddev_head;
  	int index = 0;
  	double n_tuned = 0.0;

    if (!sampling_enabled) {
      pwr_standardDeviation = 0.0;
      pwr_percentageErrorInterval = 0.0;
      pwr_recommendedK = 0;
      return; 
    }
  
  	if (!pwr_stddev_head)
  	  return; /* Avoid possible divisions by zero */
  
      while ( stddev_iter->next != NULL || index < pwr_stddev_index) {
      	double x = stddev_iter->pwr[index++] - mean_power;
          sum += x * x;
          if (index == 1024) {
             stddev_iter = stddev_iter->next;
             index = 0;
          }
      }
  	variance = sum / (pwr_sample_size - 1);
  
  	pwr_standardDeviation = sqrt(variance);
    //3.0 corresponds to the Z score for 99.7% confidence
	pwr_percentageErrorInterval = z_score * ( pwr_standardDeviation ) / sqrt( pwr_sample_size ) / ( mean_power ) * to_percent;
    n_tuned =  ( z_score * pwr_standardDeviation / mean_power / desired_error ) * ( z_score * pwr_standardDeviation / mean_power / desired_error );
    pwr_recommendedK = sim_pop_insn / sampling_munit / n_tuned; 
  }
#endif

struct simoo_stats_t {
	counter_t sim_meas_insn;
	tick_t sim_meas_cycle;
#       if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
	  counter_t sim_slip;
#       endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	counter_t sim_total_insn;
	counter_t sim_num_refs;
	counter_t sim_total_refs;
	counter_t sim_num_loads;
	counter_t sim_total_loads;
	counter_t sim_num_branches;
	counter_t sim_total_branches;
	counter_t brq_count;
	counter_t brq_fcount;
	counter_t roq_count;
	counter_t roq_fcount;
	counter_t lsq_count;
	counter_t lsq_fcount;
	counter_t intq_count;
	counter_t intq_fcount;
	counter_t fpq_count;
	counter_t fpq_fcount;
} simoo_stats_backup;

void simoo_backup_stats(void) 
{
    /* Note: sim_meas_insn & sim_meas cycles are backup up, but not restored */
    /* This is for std dev calculation */
	simoo_stats_backup.sim_meas_insn = sim_meas_insn;
	simoo_stats_backup.sim_meas_cycle = sim_meas_cycle;

#       if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
          simoo_stats_backup.sim_slip = sim_slip;
#       endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	simoo_stats_backup.sim_total_insn = sim_total_insn;
	simoo_stats_backup.sim_num_refs = sim_num_refs;
	simoo_stats_backup.sim_total_refs = sim_total_refs;
	simoo_stats_backup.sim_num_loads = sim_num_loads;
	simoo_stats_backup.sim_total_loads = sim_total_loads;
	simoo_stats_backup.sim_num_branches = sim_num_branches;
	simoo_stats_backup.sim_total_branches = sim_total_branches;
	simoo_stats_backup.brq_count = brq_count;
	simoo_stats_backup.brq_fcount = brq_fcount;
	simoo_stats_backup.roq_count = roq_count;
	simoo_stats_backup.roq_fcount = roq_fcount;
	simoo_stats_backup.lsq_count = lsq_count;
	simoo_stats_backup.lsq_fcount = lsq_fcount;
	simoo_stats_backup.intq_count = intq_count;
	simoo_stats_backup.intq_fcount = intq_fcount;
	simoo_stats_backup.fpq_count = fpq_count;
	simoo_stats_backup.fpq_fcount = fpq_fcount;

	/*
	cache_backup_stats(icache);
	cache_backup_stats(cache_il2);
	cache_backup_stats(dcache);
	cache_backup_stats(cache_dl2);
	cache_backup_stats(itlb);
	cache_backup_stats(dtlb);
	power_backup_stats();
	*/
	bpred_backup_stats(pred);
}

void simoo_restore_stats(void) 
{
#       if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
          sim_slip = simoo_stats_backup.sim_slip;
#       endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	sim_total_insn = simoo_stats_backup.sim_total_insn;
	sim_num_refs = simoo_stats_backup.sim_num_refs;
	sim_total_refs = simoo_stats_backup.sim_total_refs;
	sim_num_loads = simoo_stats_backup.sim_num_loads;
	sim_total_loads = simoo_stats_backup.sim_total_loads;
	sim_num_branches = simoo_stats_backup.sim_num_branches;
	sim_total_branches = simoo_stats_backup.sim_total_branches;
	brq_count = simoo_stats_backup.brq_count;
	brq_fcount = simoo_stats_backup.brq_fcount;
	roq_count = simoo_stats_backup.roq_count;
	roq_fcount = simoo_stats_backup.roq_fcount;
	lsq_count = simoo_stats_backup.lsq_count;
	lsq_fcount = simoo_stats_backup.lsq_fcount;
	intq_count = simoo_stats_backup.intq_count;
	intq_fcount = simoo_stats_backup.intq_fcount;
	fpq_count = simoo_stats_backup.fpq_count;
	fpq_fcount = simoo_stats_backup.fpq_fcount;

	/*
	cache_restore_stats(icache);
	cache_restore_stats(cache_il2);
	cache_restore_stats(dcache);
	cache_restore_stats(cache_dl2);
	cache_restore_stats(itlb);
	cache_restore_stats(dtlb);
	power_restore_stats();
	*/
	bpred_restore_stats(pred);
}

void switch_to_fast_forwarding(void) 
{
   if (! (simulator_state == NOT_STARTED ||	simulator_state == DRAINING)) {
   	  fatal("We should only switch to fast forwarding from NOT_STARTED or DRAINING state.");
   }
   simulator_state = FAST_FORWARDING;
}

void switch_to_warming(void) 
{
   if (! (simulator_state == FAST_FORWARDING)) {
   	  fatal("We should only switch to WARMING from FAST_FORWARDING state.");
   }
   simulator_state = WARMING;
}

void start_measuring(void) 
{
   if (! (simulator_state == FAST_FORWARDING || simulator_state == WARMING )) {
   	  fatal("We should only switch to MEASURING from FAST_FORWARDING or WARMING state.");
   }
   simulator_state = MEASURING;
   simoo_restore_stats();
}

void stop_measuring(void) 
{
   int cycles_measured;

   double total_cycle_power_cc3;
   double last_total_cycle_power_cc3;
   double power_measured;
   
   /* unsigned long insn_measured = 0; */
   if (! (simulator_state == MEASURING )) {
   	  fatal("We should only switch to DRAINING from MEASURING state.");
   }

   simulator_state = DRAINING;
   /* Log an IPC sample */
   /* insn_measured = (unsigned long)(sim_meas_insn - simoo_stats_backup.sim_meas_insn); */
   cycles_measured = (int) (sim_meas_cycle - simoo_stats_backup.sim_meas_cycle);

   /*total_cycle_power_cc3 = rename_power_cc3 + bpred_power_cc3 + lsq_power_cc3 + window_power_cc3 + regfile_power_cc3 + icache_power_cc3 + resultbus_power_cc3 + clock_power_cc3 + alu_power_cc3 + dcache_power_cc3 + dcache2_power_cc3;
   last_total_cycle_power_cc3 = power_stats_backup.rename_power_cc3 + power_stats_backup.bpred_power_cc3 + power_stats_backup.lsq_power_cc3 + power_stats_backup.window_power_cc3 + power_stats_backup.regfile_power_cc3 + power_stats_backup.icache_power_cc3 + power_stats_backup.resultbus_power_cc3 + power_stats_backup.clock_power_cc3 + power_stats_backup.alu_power_cc3 + power_stats_backup.dcache_power_cc3 + power_stats_backup.dcache2_power_cc3;
   power_measured = total_cycle_power_cc3 - last_total_cycle_power_cc3; 
*/
   newSample(cycles_measured);
   //pwr_newSample(power_measured);

   simoo_backup_stats();
}

/* Warming related macros */
#define CACHE_BADDR(cp, addr)   ((addr) & ~(cp)->blk_mask)

static md_addr_t last_dl1_tag = 0;
static md_addr_t dl1_tagset_mask;
static md_addr_t last_dtlb_tag = 0;
static md_addr_t dtlb_tagset_mask;
int last_dl1_block_dirty = 0;

#define WARM_D(cmd, addr) \
    ( \
       dcache && sampling_allwarm ? \
         (               \
            ( (last_dl1_tag == ((addr) & dl1_tagset_mask)) && (((cmd) == Read) || last_dl1_block_dirty ) ) ? \
               (                                                                                           \
                  NULL /* do nothing if tagset matches last dl1 tagset, and the op is a read or the block is already dirty */                \
               ) : ( \
                  last_dl1_tag = ((addr) & dl1_tagset_mask),  \
                  last_dl1_block_dirty = ((cmd) == Write),    \
                  dl1_extremely_fast_access((cmd), (addr)),   \
                  ( last_dtlb_tag != ((addr) & dtlb_tagset_mask) ? \
                    (                                              \
                       last_dtlb_tag = ((addr) & dtlb_tagset_mask),\
                       dtlb_extremely_fast_access(dtlb, (addr))    \
                     ) : (                                         \
                       NULL                                        \
                     )                                             \
                   )                                               \
               )                                                   \
          )                                                        \
          : NULL /* do nothing if ! sampling_allwarm */            \
     )


/*
 * reconfigure the execution engine for fast forwarding
 */

#undef SET_NPC
#undef CPC
#undef GPR
#undef SET_GPR
#undef FPR_L
#undef SET_FPR_L
#undef FPR_F
#undef SET_FPR_F
#undef FPR_D
#undef SET_FPR_D
#undef SET_HI
#undef HI
#undef SET_LO
#undef LO
#undef FCC
#undef SET_FCC
#undef FPR_Q
#undef SET_FPR_Q
#undef FPR
#undef SET_FPR
#undef FPCR
#undef SET_FPCR
#undef UNIQ
#undef SET_UNIQ
#undef READ_BYTE
#undef READ_HALF
#undef READ_WORD
#undef READ_QWORD
#undef WRITE_BYTE
#undef WRITE_HALF
#undef WRITE_WORD
#undef WRITE_QWORD
#undef SYSCALL
#undef INC_INSN_CTR
#undef ZERO_FP_REG

/* next program counter */
#define SET_NPC(EXPR)		(regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC			(regs.regs_PC)

/* general purpose registers */
#define GPR(N)			(regs.regs_R[N])
#define SET_GPR(N,EXPR)		(regs.regs_R[N] = (EXPR))

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)		(regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)	(regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)		(regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)	(regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)		(regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR)	(regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)		(regs.regs_C.hi = (EXPR))
#define HI			(regs.regs_C.hi)
#define SET_LO(EXPR)		(regs.regs_C.lo = (EXPR))
#define LO			(regs.regs_C.lo)
#define FCC			(regs.regs_C.fcc)
#define SET_FCC(EXPR)		(regs.regs_C.fcc = (EXPR))

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)						\
  (WARM_D(Read, (SRC)), (FAULT) = md_fault_none, MEM_READ_BYTE(mem, (SRC)))
#define READ_HALF(SRC, FAULT)						\
  (WARM_D(Read, (SRC)), (FAULT) = md_fault_none, MEM_READ_HALF(mem, (SRC)))
#define READ_WORD(SRC, FAULT)						\
  (WARM_D(Read, (SRC)), (FAULT) = md_fault_none, MEM_READ_WORD(mem, (SRC)))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  (WARM_D(Read, (SRC)), (FAULT) = md_fault_none, MEM_READ_QWORD(mem, (SRC)))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)					\
  (WARM_D(Write, (DST)), (FAULT) = md_fault_none, MEM_WRITE_BYTE(mem, (DST), (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)					\
  (WARM_D(Write, (DST)), (FAULT) = md_fault_none, MEM_WRITE_HALF(mem, (DST), (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
  (WARM_D(Write, (DST)), (FAULT) = md_fault_none, MEM_WRITE_WORD(mem, (DST), (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  (WARM_D(Write, (DST)), (FAULT) = md_fault_none, MEM_WRITE_QWORD(mem, (DST), (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST)	sys_syscall(&regs, mem_access, mem, INST, TRUE)

#ifndef NO_INSN_COUNT
#define INC_INSN_CTR()	sim_meas_insn++
#else /* !NO_INSN_COUNT */
#define INC_INSN_CTR()	/* nada */
#endif /* NO_INSN_COUNT */

#define ZERO_FP_REG()	/* nada... */

static struct mem_t *dec = NULL;

void predecode_init(int i)
{
  /* pre-decode text segment */
  unsigned i, num_insn = (ldinfo[i].ld_text_size + 3) / 4;

  fprintf(stderr, "** pre-decoding %u insts...", num_insn);

  /* allocate decoded text space */
  dec = mem_create("dec");

  for (i=0; i < num_insn; i++)
  {
    enum md_opcode op;
    md_inst_t inst;
    md_addr_t PC;

    /* compute PC */
    PC = ldinfo[i].ld_text_base + i * sizeof(md_inst_t);

    /* get instruction from memory */
    MD_FETCH_INST(inst, mem[i], PC);

    /* decode the instruction */
    MD_SET_OPCODE(op, inst);

    /* insert into decoded opcode space */
    MEM_WRITE_WORD(dec, PC << 1, (word_t)op);
    MEM_WRITE_WORD(dec, (PC << 1)+sizeof(word_t), inst);
  }
  fprintf(stderr, "done\n");
}

/* fastfwd COUNT instructions */
void run_fast(counter_t count)
{
  /* register allocate instruction buffer */
  register md_inst_t inst;

  /* decoded opcode */
  register enum md_opcode op;

  md_addr_t last_il1_tag = 0;
  md_addr_t il1_tagset_mask = icache ? icache->tagset_mask : 0;
  //md_addr_t last_itlb_tag = 0;
  //md_addr_t itlb_tagset_mask = itlb->tagset_mask;

  dl1_tagset_mask = dcache ? dcache->tagset_mask : 0;
  dtlb_tagset_mask = dtlb ? dtlb->tagset_mask : 0;

  //myfprintf(stderr, "switch to fast %10n @ 0x%08p: \n", sim_pop_insn, regs.regs_PC);

  do {
    /* maintain $r0 semantics */
    regs.regs_R[MD_REG_ZERO] = 0;



    /* Warm the ICACHE, if enabled */
    if (sampling_allwarm && last_il1_tag != (regs.regs_PC & il1_tagset_mask)) {
      last_il1_tag = (regs.regs_PC & il1_tagset_mask);

      icache_extremely_fast_access(icache, regs.regs_PC); 
#if 0
      if (! icache_extremely_fast_access(icache, regs.regs_PC) ) {
	l2_fast_access(Read, regs.regs_PC);
      }
      if ( last_itlb_tag != (regs.regs_PC & itlb_tagset_mask)) {
	last_itlb_tag = (regs.regs_PC & itlb_tagset_mask);
	icache_extremely_fast_access(itlb, regs.regs_PC);
      }
#endif
    }

#if 0
    /* get the next instruction to execute */
    MD_FETCH_INST(inst, mem, regs.regs_PC);

    /* decode the instruction */
    MD_SET_OPCODE(op, inst);
#else
      /* load predecoded instruction */
      op = (enum md_opcode)__UNCHK_MEM_READ(dec, regs.regs_PC << 1, word_t);
      inst =
	__UNCHK_MEM_READ(dec, (regs.regs_PC << 1)+sizeof(word_t), md_inst_t);
#endif

    /* if the instruction is in the delay slot, change the NPC */
    if (is_jump == 1)
    {
      SET_NPC(target_PC);
      is_jump = 0;
    }

    /* if previous branch likely instruction is not taken,skip this
     * instruction
     */
    if (is_annulled) {
      is_annulled = 0;
      /* execute next instruction */
      regs.regs_PC = regs.regs_NPC;
      regs.regs_NPC += sizeof(md_inst_t);
      continue;
    }

    sim_pop_insn ++;

   // if (verbose) {
    if (verbose && (sim_pop_insn % opt_inst_interval == 0)) {
      myfprintf(stderr, "%10n @ 0x%08p: ", sim_pop_insn, regs.regs_PC);
      md_print_insn(inst, regs.regs_PC, stderr);
      myfprintf(stderr, "\n");
    }

    /* execute the instruction */
    switch (op) {
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)	\
      case OP:							\
	SYMCAT(OP,_IMPL);					\
        break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)				\
      case OP:							\
	panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#undef DECLARE_FAULT
#define DECLARE_FAULT(FAULT)					\
	{ /* uncaught */ break; }
#include "mips.def"
      default:
	printf("@%08x\n",regs.regs_PC);
	panic("attempted to execute a bogus opcode");
    }

    /* WARM BPRED */
    if (sampling_allwarm && MD_OP_FLAGS(op) & F_CTRL) {
      if (MD_IS_RETURN(op)) {
	pred->retstack.tos = (pred->retstack.tos + pred->retstack.size - 1) % pred->retstack.size;
      } else {
	md_addr_t npc = (is_jump ? target_PC : regs.regs_NPC);
	bpred_update_fast(regs.regs_PC,npc,op, MD_IS_CALL(op));
      }
    }

#if 0
    {
      if (sim_pop_insn>=25000000 && sim_pop_insn <=25010000) {
	myfprintf(stderr, "%10n @ 0x%08p: ", sim_pop_insn, regs.regs_PC);
	md_print_insn(inst, regs.regs_PC, stderr);
	myfprintf(stderr, "\n");
      }
#if 0
      if ((sim_pop_insn+1)%1000==0) {
	printf("%08lx@%lld\n",regs.regs_PC,sim_pop_insn+1);
	/*
	   if (sim_pop_insn>=1000014 && sim_pop_insn < 1000017) {
	   printf("a0,a1,a3=<%lx,%lx,%lx>\n",regs.regs_R[4],regs.regs_R[5],regs.regs_R[7]);
	   }
	   */
      }
#endif
    }
#endif


    /* go to the next instruction */
    regs.regs_PC = regs.regs_NPC;
    regs.regs_NPC += sizeof(md_inst_t);

    count--;

    /* finish early? */
    if (opt_max_insts && sim_pop_insn >= opt_max_insts)
      return;

  } while (count>0);

  /* if last executed is a branch,restart from it to help switch */
  if (MD_OP_FLAGS(op) & F_CTRL) {
    sim_pop_insn --;
    regs.regs_NPC = regs.regs_PC;
    regs.regs_PC -= sizeof(md_inst_t);
    is_jump = 0;
    is_annulled = 0;
  }
  //myfprintf(stderr, "switch to warm %10n @ 0x%08p: \n", sim_pop_insn, regs.regs_PC);
}

#endif
