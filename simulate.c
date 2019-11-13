/*
 * simulate.c - sim_main for godson2 detailed simulator
 *
 * This file is written by godson cpu group.
 * It has been written by extending the alphasim tool suite written by
 * Raj Desikan,which is in turn based on simplescalar.
 *  
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
 * Copyright (C) 1999 by Raj Desikan
 *
 * Copyright (C) 2004 by Fuxin Zhang
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
#include "sampling.h"
#include "power.h"
#include "noc.h"

//#include "ptrace.h"

//#include "istat.h"

double DeviationThreshold = 0.0175;
//double ReactionChange = 0.05; //original: 0.06
double ReactionChange = 75; //original: 0.06
double Decay = 25;
double PerfDegThreshold = 0.025;
double EndstopCount = 10;

#define MINIMUM_FREQ 125//0.125
#define MAXIMUM_FREQ 1000//1.0

//#define MINIMUM_PERIOD 1//0.125
//#define MAXIMUM_PERIOD 8//1.0
#define SAMPLING_PERIOD 10000

#define MINIMUM_DUTY 1
#define MAXIMUM_DUTY 8
#define FREQ_DIVIDE 8
#define FREQ_STEP 1

/* fast forward insts before detailed simulation */
counter_t opt_fastfwd_count;

/* maximum insts to execute */
counter_t opt_max_insts;

/* verbose print inst interval */
counter_t opt_inst_interval;

/* cycle counter */
tick_t sim_cycle = 0;

int total_cpus = 1;
struct godson2_cpu cpus[MAX_CPUS];

int mesh_width = 4;
int mesh_height;
int router_ifq_size;
int router_ofq_size;

/* options: presently the same for all cpus */
int fetch_ifq_size;
/* maximum insts fetch at one cycle */
int fetch_width;
/* maximum branches fetch at one cycle */
int fetch_speed;

int bht_nsets = 512;
int bht_lognsets = 9;
int btb_size = 16;
int btb_pc_low = 3;
int btb_pc_high = 27;
int va_size = 64;
int pa_size = 40;

int decode_pipein_bits = 480; 
int decode_width = 4;
int decode_speed;
int decode_ifq_size;
int inst_width;

/* maximum instruction mapped per cycle */
int map_width = 4;
int fix_wb_width = 3;
int fp_wb_width = 2;
int map_pipein_bits = 124;
int map_pipeout_bits = 130;
int fix_phy_num = 64;
int fix_phy_lognum = 6;
int fp_phy_num = 64;
int fp_phy_lognum = 6;
int fix_state_bits = 7;
int fp_state_bits = 7;

/* total integer rename register number */
int int_rename_reg_size;
/* total floating-point rename register number */
int fp_rename_reg_size;

/* issue queue */
int issue_width;
int sim_has_bypass;
int int_issue_ifq_size = 16;
int fp_issue_ifq_size = 16;
int fix_issue_width = 3;
int fp_issue_width = 2;
int fixq_misc_bits = 62;
int fpq_misc_bits = 34;

int fix_regfile_pipein_bits = 73;
int fp_regfile_pipein_bits = 63;
int fp_fcr_reg_bits = 135;

/* functional units */
int ialu_num = 3;
int falu_num = 2;
int data_width = 64;
int ialu_pipein_bits = 286;
int ialu_pipeout_bits = 170;
int falu_pipein_bits = 166;
int falu_pipeout_bits = 170;

/* commit */
int commit_width = 4;
int roq_ifq_size = 64;
int brq_ifq_size = 8;
int roq_state_bits = 2;
int roq_bits = 64;
int brq_state_bits = 2;
int brq_bits = 170;

/* memory access */
int lsq_ifq_size = 24;
int lsq_wb_width = 1;
int lsq_state_bits = 3;
int lsq_bits = 201;

int icache_pipein_bits = 330;
int itlb_pipein_bits = 95;
int dcache_pipein_bits = 163;
int dtlb_pipein_bits = 230;
int cache_dl2_pipein_bits = 360;
int cache_dl2_sramin_bits = 346;
int cache_dl2_sramout_bits = 1210;

int refill_width = 1;
int missq_ifq_size;
int missq_bits = 356;
int wtbkq_ifq_size;
int wtbkq_bits = 333;
int extinvnq_ifq_size;
int input_buffer_bits[NUM_TYPES] = {100/PACK_SIZE,100/PACK_SIZE,300/PACK_SIZE,300/PACK_SIZE};

int missspec_penalty;
int perfect_cache;

/* simple memory delay params */
int mem_read_first_delay;
int mem_read_interval_delay;
int mem_write_delay;

int memq_ifq_size;

int io_bits = 200;

//struct refill_bus refill;

#if 0

char *dcache_name;
char *icache_name;
int  cache_nelt = 0;
char *cache_configs[MAX_NUM_CACHES];
int  num_caches;
struct cache *caches[MAX_NUM_CACHES];
int  flush_on_syscalls;
int  victim_buf_ent;
int  victim_buf_lat;

/* Name of level-one data tlb */
char *dtlb_name;
/* Name of level-one instruction tlb */
char *itlb_name;

/* TLB definition counter */
int  tlb_nelt = 0;
/* Array of tlb strings */
char *tlb_configs[MAX_NUM_TLBS];
/* Array of tlb pointers */
struct cache *tlbs[MAX_NUM_TLBS];
int  num_tlbs;

#endif

#if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
/* pipeline trace range and output filename */
static int ptrace_nelt = 0;
static char *ptrace_opts[2];

/* text-based stat profiles */
int pcstat_nelt = 0;
char *pcstat_vars[MAX_PCSTAT_VARS];

struct stat_stat_t *pcstat_stats[MAX_PCSTAT_VARS];
counter_t pcstat_lastvals[MAX_PCSTAT_VARS];
struct stat_stat_t *pcstat_sdists[MAX_PCSTAT_VARS];
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

/* total non-speculative bogus addresses seen (debug var) */
//static counter_t sim_invalid_addrs;


/* register simulator-specific options */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
		 "sim-godson: This simulator implements a detailed out of order godson2 simulator.\n\n\n");
  
  /* general simulator options */
  /* instruction limit */
  
  opt_reg_int(odb, "-cpu_num", "number of simulated cpu",
           &total_cpus, /* default */1, /* print */TRUE,
           /* format */NULL);
  
  opt_reg_int(odb, "-mesh_width", "width of simulated 2D mesh network",
           &mesh_width, /* default */4, /* print */TRUE,
           /* format */NULL);
  
  opt_reg_int(odb, "-router_ifq_size", "queue size of simulated router buffer",
           &router_ifq_size, /* default */8, /* print */TRUE,
           /* format */NULL);
  opt_reg_int(odb, "-router_ofq_size", "queue size of simulated router buffer",
           &router_ofq_size, /* default */3, /* print */TRUE,
           /* format */NULL);
  
  
  opt_reg_longint(odb, "-max:inst", "maximum number of inst's to execute",
	       &opt_max_insts, /* default */0, /* print */TRUE, 
	       /* format */NULL);

  opt_reg_longint(odb, "-inst_interval", "if verbose,print out inst every interval insts",
	       &opt_inst_interval, /* default */10000, /* print */TRUE, 
	       /* format */NULL);
#if 0
  /* sampling options */
  opt_reg_flag(odb, "-sampling", "enable systematic sampling",
      &sampling_enabled, /* default */FALSE, /* print */TRUE, NULL);

  opt_reg_int(odb, "-sampling:k",
      "for systematic sampling, the k value",
      &sampling_k, /* default */ 1000, /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-sampling:w-unit",
      "for systematic sampling, the oo-warming unit size",
      &sampling_wunit, /* default */ 2000, /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-sampling:m-unit",
      "for systematic sampling, the measurement unit size in instructions",
      &sampling_munit, /* default */ 1000, /* print */TRUE, /* format */NULL);

  opt_reg_flag(odb, "-sampling:allwarm",
      "perform in-order warming always",
      &sampling_allwarm, /* default */TRUE, /* print */TRUE, NULL);

  opt_reg_longint(odb, "-fastfwd", "number of insts skipped before timing starts",
	      &opt_fastfwd_count, /* default */0,
	      /* print */TRUE, /* format */NULL);

# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
  opt_reg_string_list(odb, "-ptrace",
      "generate pipetrace, i.e., <fname|stdout|stderr> <range>",
      ptrace_opts, /* arr_sz */2, &ptrace_nelt, /* default */NULL,
      /* !print */FALSE, /* format */NULL, /* !accrue */FALSE);

  opt_reg_note(odb,
      "  Pipetrace range arguments are formatted as follows:\n"
      "\n"
      "    {{@|#}<start>}:{{@|#|+}<end>}\n"
      "\n"
      "  Both ends of the range are optional, if neither are specified, the entire\n"
      "  execution is traced.  Ranges that start with a `@' designate an address\n"
      "  range to be traced, those that start with an `#' designate a cycle count\n"
      "  range.  All other range values represent an instruction count range.  The\n"
      "  second argument, if specified with a `+', indicates a value relative\n"
      "  to the first argument, e.g., 1000:+100 == 1000:1100.  Program symbols may\n"
      "  be used in all contexts.\n"
      "\n"
      "    Examples:   -ptrace FOO.trc #0:#1000\n"
      "                -ptrace BAR.trc @2000:\n"
      "                -ptrace BLAH.trc :1500\n"
      "                -ptrace UXXE.trc :\n"
      "                -ptrace FOOBAR.trc @main:+278\n"
      );
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
#endif
  
  /* Processor core options */
  /* The clock frequency of simulated machine */
  opt_reg_int(odb, "-mach:freq", "frequency of simulated machine",
	      &mips_cpu_freq, /* default */225000000, /* print */TRUE, /* format */NULL);
  /* fetch queue size */
  opt_reg_int(odb, "-fetch:ifqsize", "Instruction fetch queue size(in insts)", 
	      &fetch_ifq_size, /* default */4, 
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-fetch:speed","Number of discontinuous fetches per cycle", 
	      &fetch_speed, /* default */1,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-fetch:width","Number of instructions to fetch per access",
	      &fetch_width, /* default */4,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-decode:width", "Instruction slotting width(in insts)", 
	      &decode_width, /* default */4,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-decode:ifqsize", "Instruction decode queue size(in insts)", 
	      &decode_ifq_size, /* default */4, 
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-decode:speed","Number of discontinuous decode per cycle", 
	      &decode_speed, /* default */1,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-map:width", "mapping width(in insts)", 
	      &map_width, /* default */4,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-roq:ifqsize", "Instruction reorder queue size(in insts)", 
	      &roq_ifq_size, /* default */64, 
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-brq:ifqsize", "Instruction branch queue size", 
	      &brq_ifq_size, /* default */8, 
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-map:intreg", "Number of integer physical rename registers", 
	      &int_rename_reg_size, /* default */29, 
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-map:fpreg", "Number of floating-point physical rename registers", 
	      &fp_rename_reg_size, /* default */32, 
	      /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-issue:width", "issue width(in insts)", 
	      &issue_width, /* default */5,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-issue:bypass", "have bypass network?", 
	      &sim_has_bypass, /* default */0,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-issue:intqueue", "Integer inst issue queue size", 
	      &int_issue_ifq_size, /* default */16,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-issue:fpqueue", "fp inst issue queue size", 
	      &fp_issue_ifq_size, /* default */16,
	      /* print */TRUE, /* format */NULL);
#if 0
  opt_reg_int(odb, "-issue:int_reg_lat", "Latency of integer register read",
	      &int_reg_read_latency, /* default */ 1,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-issue:fp_reg_lat", "Latency of fp register read",
	      &fp_reg_read_latency, /* default */ 1,
	      /* print */ TRUE, /* format */NULL);
#endif

  opt_reg_int(odb, "-commit:width", "commit width(in insts)", 
	      &commit_width, /* default */4,
	      /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-missq:ifqsize", "miss queue size", 
	      &missq_ifq_size, /* default */8,
	      /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-wtbkq:ifqsize", "miss queue size", 
	      &wtbkq_ifq_size, /* default */8,
	      /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-extinvnq:ifqsize", "external request queue size", 
	      &extinvnq_ifq_size, /* default */2,
	      /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-memq:ifqsize", "memory queue size", 
	      &memq_ifq_size, /* default */1,
	      /* print */TRUE, /* format */NULL);

# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  opt_reg_string_list(odb, "-pcstat",
      "profile stat(s) against text addr's (mult uses ok)",
      pcstat_vars, MAX_PCSTAT_VARS, &pcstat_nelt, NULL,
      /* !print */FALSE, /* format */NULL, /* accrue */TRUE);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  opt_reg_int(odb, "-mem:lsq_ifq_size", "load/store queue size", 
	      &lsq_ifq_size, /* default */32, 
	      /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-miss_penalty", "load speculation miss penalty", 
	      &missspec_penalty, /* default */100, 
	      /* print */TRUE, /* format */NULL);

  /* Memory hierarchy options */
  /* cache options */
  opt_reg_string_list(odb, "-cache:define", "cache configuration",
		      cache_configs, MAX_NUM_CACHES, &cache_nelt, NULL,
		      /* print */TRUE, /* format */NULL, /* accrue */TRUE);
  opt_reg_note(odb,
	       "  The cache config parameter <config> has the following format:\n"
	       "\n"
	       "    <name>:<nsets>:<bsize>:<subblock>:<asso>:<repl>:<lat>:<trans>:<# resources>:<res code>"
	       "\n"
	       "    <name>   - name of the cache being defined\n"
	       "    <nsets>  - number of sets in the cache\n"
	       "    <bsize>  - block size of the cache\n"
	       "    <assoc>  - associativity of the cache\n"
	       "    <repl>   - block replacement strategy, 'l'-LRU, 'f'-FIFO, 'r'-random, 'F'-LFU\n"
	       "    <lat>    - hit latency\n"
	       "    <trans>  - Translation policy, vivt, vipt, pipt\n"
	       "    <pref>   - prefetch enabled if 1\n"
	       "\n"
	       "    Examples:   -cache:define           DL1:512:64:0:2:F:3:vipt:0:1"
	       "                -dtlb dtlb:128:4096:32:r\n"
	       );
  opt_reg_flag(odb, "-cache:flush", "flush caches on system calls",
	       &flush_on_syscalls, /* default */FALSE, /* print */TRUE, 
	       NULL);

  opt_reg_string(odb, "-cache:dcache",
		 "defines name of first-level data cache",
		 &dcache_name, /* default */NULL,
		 /* print */TRUE, /* format */NULL);

  opt_reg_string(odb, "-cache:icache",
		 "defines name of first-level instruction cache",
		 &icache_name, /* default */NULL,
		 /* print */TRUE, /* format */NULL);

  opt_reg_string(odb, "-cache:cache_dl2",
		 "defines name of two-level unified cache",
		 &cache_dl2_name, /* default */NULL,
		 /* print */TRUE, /* format */NULL);
  
  opt_reg_int(odb, "-cache:vbuf_lat", 
	      "Additional victim buffer latency", 
	      &victim_buf_lat, /* default */ 1,
	      /* print */TRUE, /* format */NULL);
  
  opt_reg_int(odb, "-cache:vbuf_ent",
	      "Number of entries in the victim buffer",
	      &victim_buf_ent, /* default */ 8,
	      /* print */TRUE, /* format */NULL);
  
  opt_reg_int(odb, "-cache:mshrs",
		 "Sets maximum number of MSHRs per cache",
		 &regular_mshrs, /* default */8,
		 /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-cache:prefetch_mshrs",
		 "Sets maximum number of MSHRs per cache",
		 &prefetch_mshrs, /* default */2,
		 /* print */TRUE, /* format */NULL);

  opt_reg_int(odb, "-cache:mshr_targets",
		 "Sets number of allowable targets per mshr",
		 &mshr_targets, /* default */4,
		 /* print */TRUE, /* format */NULL);
  

		 /* bus options */
  opt_reg_string_list(odb, "-bus:define", "bus configuration",
		 bus_configs, MAX_NUM_BUSES, &bus_nelt, NULL,
		 /* print */TRUE, /* format */NULL, /* accrue */TRUE);
	      
  /* mem options */
  opt_reg_int(odb, "-mem_read_first_delay", "cycles needed to return first block for reads", 
	      &mem_read_first_delay, 1, TRUE, NULL);

  opt_reg_int(odb, "-mem_read_interval_delay", "cycles needed to between subblocks returned", 
	      &mem_read_interval_delay, 1, TRUE, NULL);

  opt_reg_int(odb, "-mem_write_delay", "cycles needed to perform a memory write", 
	      &mem_write_delay, 1, TRUE, NULL);

  opt_reg_string_list(odb, "-mem:define", "memory bank name",
		 mem_configs, MAX_NUM_MEMORIES, &mem_nelt, NULL,
		 /* print */TRUE, /* format */NULL, /* accrue */TRUE);
  opt_reg_int(odb, "-mem:queuing_delay", "Queuing delay enabled in memory",
	      &mem_queuing_delay, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-bus:queuing_delay", "Queuing delay enabled in buses",
	      &bus_queuing_delay, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  /* Options for Vinodh Cuppu's code */
  opt_reg_int(odb, "-mem:clock_multiplier",
	      "cpu freq / dram freq", &clock_multiplier, 3, TRUE, NULL);
  opt_reg_int(odb, "-page_policy", "0 - openpage, 1 - closepage autoprecharge", 
	      &page_policy, 1, TRUE, NULL);
  opt_reg_int(odb, "-mem:ras_delay", "time between start of ras command and cas command",
	      &ras_delay, 3, TRUE, NULL);
  opt_reg_int(odb, "-mem:cas_delay", "time between start of cas command and data start",
	      &cas_delay, 3, TRUE, NULL);
  opt_reg_int(odb, "-mem:pre_delay", "time between start of precharge command and ras command",
	      &pre_delay, 3, TRUE, NULL);
  opt_reg_int(odb, "-mem:data_rate", "1 - single data rate. 2 - double data rate",
	      &data_rate, 1, TRUE, NULL);
  opt_reg_int(odb, "-mem:bus_width", "width of bus from cpu to dram",
	      &bus_width, 8, TRUE, NULL);
  opt_reg_int(odb, "-mem:chipset_delay_req", "delay in chipset for request path",
	      &chipset_delay_req, 2, TRUE, NULL);
  opt_reg_int(odb, "-mem:chipset_delay_return", "delay in chipset in data return path",
	      &chipset_delay_return, 2, TRUE, NULL);
  
/* End of remove block - HRISHI */
	      
  /* TLB options */

  opt_reg_string_list(odb, "-tlb:define",
		 "tlb configuration",
		 tlb_configs, MAX_NUM_TLBS, &tlb_nelt, NULL,
		 /* print */TRUE, /* format */NULL, /* accrue */TRUE);

  opt_reg_string(odb, "-tlb:itlb",
		 "Name of L1 ITLB",
		 &itlb_name, NULL, /* print */TRUE, NULL);

  opt_reg_string(odb, "-tlb:dtlb",
		 "name of L2 dtlb",
		 &dtlb_name, NULL, /* print */TRUE, NULL);
  
#if 0
  /* Predictor options */
  opt_reg_int(odb, "-bpred:line_pred", 
	      "Line predictor", 
	      &line_predictor, /* default */ TRUE,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-line_pred:ini_value", "Initial value of line pred bits",
	      &line_pred_ini_value, /* default */ 0,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-line_pred:width", "Line predictor width",
	      &line_pred_width, /* default */ 4,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-way:pred",
	      "Way predictor latency",
	      &way_pred_latency, /* default */1,
	      /* print */TRUE, /* format */NULL);
#endif
  
  /* branch predictor options */
  opt_reg_note(odb, 
	       "  Branch predictor configuration examples for 2-level predictor:\n"
	       "  Configurations:   N, M, W, X\n"
	       "   N   # entries in first level (# of shift register(s))\n"
	       "   W   width of shift register(s)\n"
	       "   M   # entries in 2nd level (# of counters, or other FSM)\n"
	       "   X   (yes-1/no-0) xor history and address for 2nd level index\n"
	       "  Sample predictors:\n"
	       "   GAg     : 1, W, 2^W, 0\n"
	       "   GAp     : 1, W, M (M > 2^W), 0\n"
	       "   PAg     : N, W, 2^W, 0\n"
	       "   PAp     : N, W, M (M == 2^(N+W)), 0\n"
	       "   gshare  : 1, W, 2^W, 1\n"
	       "   Predictor `comb' combines a bimodal and a 2-level predictor.\n"
	       );

  opt_reg_string(odb, "-bpred",
		 "branch predictor type {nottaken|taken|perfect|bimod|2lev|comb|21264}",
		 &pred_type, /* default */"2lev",
		 /* print */TRUE, /* format */NULL);

  opt_reg_int_list(odb, "-bpred:bimod",
		   "bimodal predictor config (<table size>)",
		   bimod_config, bimod_nelt, &bimod_nelt,
		   /* default */bimod_config,
		   /* print */TRUE, /* format */NULL, /* !accrue */FALSE);
  opt_reg_int_list(odb, "-bpred:2lev",
		   "2-level predictor config "
		   "(<l1size> <l2size> <hist_size> <xor>)",
		   twolev_config, twolev_nelt, &twolev_nelt,
		   /* default */twolev_config,
		   /* print */TRUE, /* format */NULL, /* !accrue */FALSE);

  opt_reg_int_list(odb, "-bpred:21264",
		   "21264 predictor config "
		   "(<l1size> <l2size> <lhist_size> <gsize> <ghist_size> <csize> <chist_size>)",
		   pred_21264_config, pred_21264_nelt, &pred_21264_nelt,
		   /* default */pred_21264_config,
		   /* print */TRUE, /* format */NULL, /* !accrue */FALSE);

  opt_reg_int_list(odb, "-bpred:comb",
		   "combining predictor config (<meta_table_size>)",
		   comb_config, comb_nelt, &comb_nelt,
		   /* default */comb_config,
		   /* print */TRUE, /* format */NULL, /* !accrue */FALSE);
  opt_reg_int(odb, "-bpred:ras",
              "return address stack size (0 for no return stack)",
              &ras_size, /* default */4,
              /* print */TRUE, /* format */NULL);

  opt_reg_int_list(odb, "-bpred:btb",
		   "BTB config (<num_sets> <associativity>)",
		   btb_config, btb_nelt, &btb_nelt,
		   /* default */btb_config,
		   /* print */TRUE, /* format */NULL, /* !accrue */FALSE);
  
#if 0
  /* Alpha 21264 specific low level feature options */
  /* st wait table size */
  opt_reg_int(odb, "-fetch:stwait", "size of st wait table (0 for no table)", 
	      &fetch_st_table_size, /* default */1024,
	      /* print */ TRUE, /* format */NULL);

  opt_reg_int(odb, "-line_pred:spec_update", "Line predictor speculative update",
	      &line_pred_spec_update, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-bpred:spec_update", "branch predictor speculative update",
	      &bpred_spec_update, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-issue:no_slot_clus", "disable slotting and clustering",
	      &issue_no_slot_clus, /* default */ FALSE,
	      /* print */ TRUE, /* format */NULL);
  
  opt_reg_int(odb, "-slot:adder", "Adder for computing branch targets",
	      &slot_adder, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-slot:slotting", "Whether to use static slotting",
	      &static_slotting, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-map:early_retire", "Early inst. retire enabled",
	      &early_inst_retire, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-wb:load_trap", "Load traps enabled",
	      &load_replay_trap, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-wb:diffsize_trap", "Different size traps enabled",
	      &diffsize_trap, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-cache:target_trap", "Trap if two loads map to same MSHR target",
	      &cache_target_trap, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
  opt_reg_int(odb, "-cache:addr_trap", "Trap if two loads map to same cache line but have different addresses",
	      &cache_diff_addr_trap, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
   opt_reg_int(odb, "-cache:mshrfull_trap", "Trap if MSHRs are full",
	      &cache_mshrfull_trap, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
   opt_reg_int(odb, "-map:stall", "Stall for 3 cycles of map < 8 free regs",
	      &map_stall, /* default */ TRUE,
	      /* print */ TRUE, /* format */NULL);
   opt_reg_int(odb, "-cache:perfectl2", "simulate perfect L2 cache",
	      &perfectl2, /* default */ FALSE,
	      /* print */ TRUE, /* format */NULL);   
  opt_reg_int(odb, "-load:spec",
	      "Use load use speculation",
	      &wb_load_use_speculation, /* default */TRUE,
	      /* print */TRUE, /* format */NULL);
  opt_reg_int(odb, "-prefetch:dist",
	      "Number of blocks to prefetch on a icache miss",
	      &prefetch_dist, /* default */4,
	      /* print */TRUE, /* format */NULL);
#endif

}

/* --------------------------------------------------------------- */
/* Memory hierarchy setup code */

void *scan_resources(char *name, enum resource_type *type)
{
  int i;

  for (i=0; i<num_caches; i++)
  {
    if (!strcmp(name, caches[i]->name)) {
      *type = Cache;
      return((void *) caches[i]);
    }
  }
  for (i=0; i<num_tlbs; i++)
  {
    if (!strcmp(name, tlbs[i]->name)) {
      *type = Cache;
      return((void *) tlbs[i]);
    }
  }
  for (i=0; i<num_buses; i++)
  {
    if (!strcmp(name, buses[i]->name))
    {
      *type = Bus;
      return((void *) buses[i]);
    }
  }
  for (i=0; i<num_mem_banks; i++)
  {
    if (!strcmp(name, mem_banks[i]->name))
    {
      *type = Memory;
      return((void *) mem_banks[i]);
    }
  }
  return((void *) NULL);
}


void create_all_buses()
{
  int i, j, width, arbitration, inf_bandwidth, resources, resource_code;
  float proc_cycles;
  char name[128];
  char *resource_name[MAX_NUM_RESOURCES];

  for (i=0; i<num_buses; i++)
    {
      if (sscanf(bus_configs[i], "%[^:]:%d:%f:%d:%d:%d:%d", name, &width, 
		 &proc_cycles, &arbitration, &inf_bandwidth, &resources, 
		 &resource_code) != 7)
	fatal("bad bus parameters: <name>:<width>:<cycle latency>:<arbitration penalty>:<inf bandwidth>:<# resources>:<resource code>:[resource names]*");
      process_resources(bus_configs[i], resource_name, resources, 7);
      buses[i] = bus_create(name, width, proc_cycles, arbitration, 
				inf_bandwidth, resources, resource_code, 
				resource_name); 
      for (j=0; j<resources; j++) 
    	free(resource_name[j]);
    }
}


void create_all_caches()
{
  int i, j;
  char name[128], repl, *resource_name[MAX_NUM_RESOURCES];
  char trans[5];
  int nsets, bsize, assoc, hitl, prefetch, resources, resource_code, subblock;

  for (i=0; i<num_caches; i++)
    {
      if (sscanf(cache_configs[i], "%[^:]:%d:%d:%d:%d:%c:%d:%[^:]:%d:%d:%d",
		 name, &nsets, &bsize, &subblock, &assoc, &repl, &hitl, 
		 trans, &prefetch, &resources, &resource_code) != 11)
	fatal("bad cache parms: <name>:<nsets>:<bsize>:<subblock>:<assoc>:<repl>:<hitlatency>:<translation>:<prefetch>:<# resources>:<resource code>:[resource names]*");
      /* Read in resource names, have to do this later since # of names are variable */
      process_resources(cache_configs[i], resource_name, resources, 11);

      /* tmp hack */
      if (!strcmp(name,dcache_name)) {
        for (j=0; j<total_cpus; j++) {
    	  caches[i*total_cpus+j] = cpus[j].dcache = cache_timing_create(name, nsets, bsize, subblock, 
				      /*balloc*/ FALSE,
				      /*usize*/ 0, assoc, hitl, 
				      cache_char2policy(repl), 
				      cache_string2trans(trans), 
				      prefetch, resources, resource_code,
				      resource_name); 
    	  cpus[j].dcache->owner = (struct godson2_cpu *)&cpus[j];
    	}
      }else if (!strcmp(name,icache_name)) {
        for (j=0; j<total_cpus; j++) {
    	  caches[i*total_cpus+j] = cpus[j].icache = cache_timing_create(name, nsets, bsize, subblock, 
				      /*balloc*/ FALSE,
				      /*usize*/ 0, assoc, hitl, 
				      cache_char2policy(repl), 
				      cache_string2trans(trans), 
				      prefetch, resources, resource_code,
				      resource_name); 
    	  cpus[j].icache->owner = (struct godson2_cpu *)&cpus[j];
    	}
      }else if (!strcmp(name,cache_dl2_name)) {
        for (j=0; j<total_cpus; j++) {
    	  caches[i*total_cpus+j] = cpus[j].cache_dl2 = cache_timing_create(name, nsets, bsize, subblock, 
				      /*balloc*/ FALSE,
				      /*usize*/ 0, assoc, hitl, 
				      cache_char2policy(repl), 
				      cache_string2trans(trans), 
				      prefetch, resources, resource_code,
				      resource_name); 
    	  cpus[j].cache_dl2->owner = (struct godson2_cpu *)&cpus[j];
		}
      }
            
      for (j=0; j<resources; j++) 
    	free(resource_name[j]);
    }
}

void create_tlbs()
{
  int i, j;
  char name[128], repl, *resource_name[MAX_NUM_RESOURCES];
  char trans[5];
  int nsets, bsize, assoc, hitl, resources, resource_code, prefetch, subblock;
  
  for (i=0; i<num_tlbs; i++)
    {
      if (sscanf(tlb_configs[i], "%[^:]:%d:%d:%d:%d:%c:%d:%[^:]:%d:%d:%d",
		 name, &nsets, &bsize, &subblock, &assoc, &repl, &hitl, trans, 
		 &prefetch, &resources, &resource_code) != 11)
	fatal("bad tlb parms: <name>:<nsets>:<bsize>:<subblock>:<assoc>:<repl>:<hitlatency>:<translation>:<prefetch>:<# resources>:<resource code>:[resource names]*");

      /* Read in resource names, have to do this later since # of names are variable */
      process_resources(tlb_configs[i], resource_name, resources, 11);

      /* tmp hack */
      if (!strcmp(name,dtlb_name)) {
        for (j=0; j<total_cpus; j++) {
    	  tlbs[i*total_cpus+j] = cpus[j].dtlb = cache_timing_create(name, nsets, bsize, subblock, 
				      /*balloc*/ FALSE,
				      /*usize*/ 0, assoc, hitl, 
				      cache_char2policy(repl), 
				      cache_string2trans(trans), 
				      prefetch, resources, resource_code,
				      resource_name); 
    	  cpus[j].dtlb->owner = (struct godson2_cpu *)&cpus[j];
    	}
      }else if (!strcmp(name,itlb_name)) {
        for (j=0; j<total_cpus; j++) {
    	  tlbs[i*total_cpus+j] = cpus[j].itlb = cache_timing_create(name, nsets, bsize, subblock, 
				      /*balloc*/ FALSE,
				      /*usize*/ 0, assoc, hitl, 
				      cache_char2policy(repl), 
				      cache_string2trans(trans), 
				      prefetch, resources, resource_code,
				      resource_name); 
    	  cpus[j].itlb->owner = (struct godson2_cpu *)&cpus[j];
    	}
      }
	  
      for (j=0; j<resources; j++) 
    	free(resource_name[j]);
    }
}


void create_all_mem_banks()
{
  int i;
  char name[128];

  for (i=0; i<num_mem_banks; i++)
    {
      if (sscanf(mem_configs[i], "%[^:]", name) != 1)
	fatal("bad memory parameters: <name> - %s", mem_configs[i]);
      mem_banks[i] = mem_bank_create(name);
    }
}


void link_memory_hierarchy()
{
  int i, j;
  struct cache *cp;
  struct bus *bp;
  enum resource_type type;

  for (i=0; i<num_caches; i++)
  {
    cp = caches[i];

    for (j=0; j<cp->num_resources; j++)
    {
      cp->resources[j] = scan_resources(cp->resource_names[j], &type);
      if (!cp->resources[j])
      {
    	fatal("Can't find resource name %s for cache %s\n", 
	      cp->resource_names[j], cp->name);
      }
      cp->resource_type[j] = type;

      /* This is a hack to link in the TLBs.  We assume that the dtlb is always
	 used in the hierarchy (wherever address translation needs to be done)
	 unless the cache is the icache and there is an itlb explicitly 
	 defined */

      /*if (cp->trans != VIVT)
    	cp->tlb = ((cp == icache) && itlb) ? itlb : dtlb;*/
      free(cp->resource_names[j]);
    }  
  }

  for (i=0; i<num_tlbs; i++)
  {
    cp = tlbs[i];

    for (j=0; j<cp->num_resources; j++)
    {
      cp->resources[j] = scan_resources(cp->resource_names[j], &type);
      if (!cp->resources[j])
      {
    	fatal("Can't find resource name %s for tlb %s\n", 
	      cp->resource_names[j], cp->name);
      }
      cp->resource_type[j] = type;
      free(cp->resource_names[j]);
    }  
  }

  for (i=0; i<num_buses; i++)
  {
    bp = buses[i];
    for (j=0; j<bp->num_resources; j++)
    {
      bp->resources[j] = scan_resources(bp->resource_names[j], &type);
      if (!bp->resources[j])
      {
    	fatal("Can't find resource name %s for bus %s\n", 
	      bp->resource_names[j], bp->name);
      }
      bp->resource_type[j] = type;
      free(bp->resource_names[j]);
    }  
  }
}


/* check simulator-specific option values */
void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  int i;
  struct opt_opt_t *an_opt;

#if 0  
  /* Verify sampling parameters */
  if (sampling_enabled) {
     if (sampling_k <= 0) {
        fatal("sampling:k may not be 0");
     }
     if (sampling_munit <= 0) {
        fatal("sampling:munit may not be 0");
     }
     if (sampling_wunit+sampling_munit > sampling_k*sampling_munit) {
        fatal("sampling:k * sampling:munit may not exceed sampling:munit + sampling:wunit");
     }
  }else {
    sampling_allwarm = FALSE;
  }
#endif
  
  /* Check if clock frequency of simulated machine is specified correctly */
  if (mips_cpu_freq < 0)
    fatal("Frequency of simulated machine should be positive");

  /* check for bpred options */
  if (!mystricmp(pred_type, "perfect"))
  {
    /* perfect predictor */
    pred_perfect =TRUE;
    /* this is not really used,but to avoid if !pred_perfect everywhere */
	for (i=0 ; i<total_cpus ; i++)
      cpus[i].pred = bpred_create(BPredNotTaken, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  }/* end if */
  else if (!mystricmp(pred_type, "taken"))
  {
    /* static predictor, taken */
	for (i=0 ; i<total_cpus ; i++)
      cpus[i].pred = bpred_create(BPredTaken, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  }/* end else if */
  else if (!mystricmp(pred_type, "nottaken"))
  {
    /* static predictor, nottaken */
	for (i=0 ; i<total_cpus ; i++)
      cpus[i].pred = bpred_create(BPredNotTaken, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  }/* end else if */
  else if (!mystricmp(pred_type, "bimod"))
  {
    /* bimodal predictor, bpred_create() checks BTB_SIZE */
    if (bimod_nelt != 1)
      fatal("bad bimod predictor config (<table_size)");
    if (btb_nelt != 2)
      fatal("bad btb config (<num_sets> <associativity>)");

	for (i=0 ; i<total_cpus ; i++)
      cpus[i].pred = bpred_create(BPred2bit,
			/* bimod table size */bimod_config[0],
			/* 2lev l1 size */0,
			/* 2lev l2 size */0,
			/* meta table size */0,
			/* history reg size */0,
			/* history xor address */0,
			/* btb stes */btb_config[0],
			/* btb assoc */btb_config[1],
			/* ret-addr stack size */ras_size,
			0,
			0,
			0,
			0);
  }/* end else if */
  else if (!mystricmp(pred_type, "2lev"))
  {
    /* 2-level adaptive predictor, bpred_create() checks args */
    if (twolev_nelt != 4)
      fatal("bad   2-level pred config (<l1size> <l2size> <hist_size> <xor>)");
    if (btb_nelt != 2)
      fatal("bad btb config (<num_sets> <associativity>)");

	for (i=0 ; i<total_cpus ; i++)
      cpus[i].pred = bpred_create(BPred2Level,
			/* bimod table size */0,
			/* 2lev l1 size */twolev_config[0],
			/* 2lev l2 size */twolev_config[1],
			/* meta table size */0,
			/* history reg size */twolev_config[2],
			/* history xor address */twolev_config[3],
			/* btb stes */btb_config[0],
			/* btb assoc */btb_config[1],
			/* ret-addr stack size */ras_size,
			0,
			0,
			0,
			0);
  }/* end else if */
#if 0
  else if (!mystricmp(pred_type, "21264"))
  {
    /* 21264 predictor, bpred_create() checks args */
    if (pred_21264_nelt != 7)
      fatal("bad   21264 pred config (<l1size> <l2size> <lhist_size> <gsize> <ghist_size> <csize> <chist_size>)");

    pred = bpred_create(BPred21264,
			/* bimod table size */0,
			/* 21264 l1 size */pred_21264_config[0],
			/* 21264 l2 size */pred_21264_config[1],
			/* meta table size */0,
			/* local history reg size */pred_21264_config[2],
			/* history xor address */0,
			/* btb stes */btb_config[0],
			/* btb assoc */btb_config[1],
			/* ret-addr stack size */ras_size,
			/* 21264 gsize */pred_21264_config[3],
			/* 21264 ghist_size */pred_21264_config[4],
			/* 21264 csize */pred_21264_config[5],
			/* 21264 chist_size */pred_21264_config[6]);
  }/* end else if */
#endif
  else if (!mystricmp(pred_type, "comb"))
  {
    /* combining predictor, bpred_create() checks args */
    if (twolev_nelt != 4)
      fatal("bad   2-level pred config (<l1size> <l2size> <hist_size> <xor>)");
    if (bimod_nelt != 1)
      fatal("bad bimod predictor config (<table_size)");
    if (comb_nelt != 1)
      fatal("bad combining predictor config (<meta_table_size>)");
    if (btb_nelt != 2)
      fatal("bad btb config (<num_sets> <associativity>)");

	for (i=0 ; i<total_cpus ; i++)
      cpus[i].pred = bpred_create(BPredComb,
			/* bimod table size */bimod_config[0],
			/* 2lev l1 size */twolev_config[0],
			/* 2lev l2 size */twolev_config[1],
			/* meta table size */comb_config[0],
			/* history reg size */twolev_config[2],
			/* history xor address */twolev_config[3],
			/* btb stes */btb_config[0],
			/* btb assoc */btb_config[1],
			/* ret-addr stack size */ras_size,
			0,
			0,
			0,
			0);
  }/* end else if */
  else
    fatal("cannot parse predictor type `%s'", pred_type);

  /* check for processor options */
  if (fetch_ifq_size < 1 || (fetch_ifq_size & (fetch_ifq_size - 1)) != 0)
    fatal("inst fetch queue size must be positive > 0 and a power of two");
  if (fetch_width < 1 || (fetch_width & (fetch_width - 1)) != 0)
    fatal("inst fetch width must be positive > 0 and a power of two");
  if (fetch_width > fetch_ifq_size)
    fatal("fetch width cannot be greater than fetch queue size");

  if (decode_width < 1 || (decode_width & (decode_width - 1)) != 0)
    fatal("decode width must be positive > 0 and a power of two");

  if (map_width < 1 || (map_width & (map_width - 1)) != 0)
    fatal("map width must be positive > 0 and a power of two");
  
  if (commit_width < 1)
    fatal("commit width must be positive > 0");
  
  /*if (fetch_st_table_size < 1 || (fetch_st_table_size & (fetch_st_table_size - 1)) != 0)
    fatal ("St wait table size must be positive > 0 and a power of two");*/
  
  an_opt = opt_find_option(odb, "-cache:define");
  num_caches = *(an_opt->nelt);
  create_all_caches();

  an_opt = opt_find_option(odb, "-tlb:define");
  num_tlbs = *(an_opt->nelt);
  create_tlbs();

#if 0  
  /* Link L1 i and d tlbs */
  if (dtlb_name && (mystricmp(dtlb_name, "none")))
    {
      for (i=0; i<num_tlbs; i++)
	if (!mystricmp(tlbs[i]->name, dtlb_name))
	  {
	    dtlb = tlbs[i];
	    break;
	  }
      if (!dtlb)
    	fatal("L1 dtlb defined but not found in tlb list");
    }

  if (itlb_name && (mystricmp(itlb_name, "none")))
    {
      for (i=0; i<num_tlbs; i++)
	if (!mystricmp(tlbs[i]->name, itlb_name))
	  {
	    itlb = tlbs[i];
	    break;
	  }
      if (!itlb)
    	fatal("L1 itlb defined but not found in tlb list");
    }

  /* Link L1 i and d caches */
  if (dcache_name && (mystricmp(dcache_name, "none")))
    {
      for (i=0; i<num_caches; i++)
	if (!mystricmp(caches[i]->name, dcache_name))
	  {
	    dcache = caches[i];
	    break;
	  }
      if (!dcache)
    	fatal("L1 dcache defined but not found in cache list");

      /* Link the dcache to the dtlb, or to the itlb if no dtlb exists */
      if (dtlb)
    	dcache->tlb = dtlb;
      else if (itlb)
    	dcache->tlb = itlb;
    }

  if (icache_name && (mystricmp(icache_name, "none")))
    {
      for (i=0; i<num_caches; i++)
	if (!mystricmp(caches[i]->name, icache_name))
	  {
	    icache = caches[i];
	    break;
	  }
      if (!icache)
    	fatal("L1 icache defined but not found in cache list");

      /* Link the icache to the itlb, or to the dtlb if no itlb exists */
      if (itlb)
    	icache->tlb = itlb;
      else if (dtlb)
    	icache->tlb = dtlb;
    }

  if (cache_dl2_name && (mystricmp(cache_dl2_name, "none")))
    {
      for (i=0; i<num_caches; i++)
	if (!mystricmp(caches[i]->name, cache_dl2_name))
	  {
	    cache_dl2 = caches[i];
	    break;
	  }
      if (!cache_dl2)
    	fatal("L2 cache defined but not found in cache list");

      /* Link the cache_dl2 to the itlb, or to the dtlb if no itlb exists */
      if (itlb)
    	cache_dl2->tlb = itlb;
      else if (dtlb)
    	cache_dl2->tlb = dtlb;
    }
#endif
  
  an_opt = opt_find_option(odb, "-bus:define");
  num_buses = *(an_opt->nelt);
  create_all_buses();

  an_opt = opt_find_option(odb, "-mem:define");
  num_mem_banks = *(an_opt->nelt);
  create_all_mem_banks();

  if ((regular_mshrs > MAX_REGULAR_MSHRS) || (regular_mshrs <= 0))
    fatal("number of regular mshrs must be 0 < x <= MAX_REGULAR_MSHRS");
  if ((prefetch_mshrs > MAX_PREFETCH_MSHRS) 
      || (prefetch_mshrs <= 0))
    fatal("number of prefetch mshrs must be 0 < x <= MAX_PREFETCH_MSHRS");
  if ((mshr_targets > MAX_TARGETS) || (mshr_targets <= 0))
    fatal("number of mshr targets must be 0 < x <= MAX_TARGETS");

  /* Link tlbs, caches, buses, and memory banks together */
  //link_memory_hierarchy();
  /* Parameters for caches, buses, and banks are checked in their respective
     create functions (cache_timing_create, bus_create, bank_create) */
  /* Check line predictor */
#if 0
  if (line_predictor && !icache)
    fatal("Line predictor can be defined only with an I-cache");
#endif
}


/* register simulator-specific statistics */
void
sim_reg_stats(struct stat_sdb_t *sdb)
{
  int i;
  //char buf[512], buf1[512];
  /* register baseline stats */
  for (i=0;i<total_cpus;i++) {
	cpus[i].sdb = stat_new();
	
	cpus[i].l1dmiss_dist = stat_reg_dist(cpus[i].sdb, "L1_miss_address_dist", "L1 Miss Address Distribution",
										 0, 4, 1, PF_ALL, NULL, NULL, NULL);

	stat_reg_counter(cpus[i].sdb, "pack_sent", "packet sent of type REQ",
					 &(cpus[i].router->pack_sent[REQ]),  0, NULL);
	stat_reg_counter(cpus[i].sdb, "pack_sent", "packet sent of type INVN",
					 &(cpus[i].router->pack_sent[INVN]), 0, NULL);
	stat_reg_counter(cpus[i].sdb, "pack_sent", "packet sent of type WTBK",
					 &(cpus[i].router->pack_sent[WTBK]), 0, NULL);
	stat_reg_counter(cpus[i].sdb, "pack_sent", "packet sent of type RESP",
					 &(cpus[i].router->pack_sent[RESP]), 0, NULL);
	
	stat_reg_counter(cpus[i].sdb, "pack_latency", "packet latency of type REQ",
					 &(cpus[i].router->pack_latency[REQ]),  0, NULL);
	stat_reg_counter(cpus[i].sdb, "pack_latency", "packet latency of type INVN",
					 &(cpus[i].router->pack_latency[INVN]), 0, NULL);
	stat_reg_counter(cpus[i].sdb, "pack_latency", "packet latency of type WTBK",
					 &(cpus[i].router->pack_latency[WTBK]), 0, NULL);
	stat_reg_counter(cpus[i].sdb, "pack_latency", "packet latency of type RESP",
					 &(cpus[i].router->pack_latency[RESP]), 0, NULL);
	
	stat_reg_double(cpus[i].sdb, "avg_latency", "average packets latency of TYPE REQ(in cycles)",
					&(cpus[i].router->avg_pack_latency[REQ]),  0., NULL);
	stat_reg_double(cpus[i].sdb, "avg_latency", "average packets latency of TYPE INVN(in cycles)",
					&(cpus[i].router->avg_pack_latency[INVN]), 0., NULL);
	stat_reg_double(cpus[i].sdb, "avg_latency", "average packets latency of TYPE WTBK(in cycles)",
					&(cpus[i].router->avg_pack_latency[WTBK]), 0., NULL);
	stat_reg_double(cpus[i].sdb, "avg_latency", "average packets latency of TYPE RESP(in cycles)",
					&(cpus[i].router->avg_pack_latency[RESP]), 0., NULL);
	
	stat_reg_double(cpus[i].sdb, "avg_latency", "average packets latency(in cycles)",
					&(cpus[i].router->avg_latency), 0., NULL);

	/* for processor consistency */
	stat_reg_counter(cpus[i].sdb, "load_mispec_rep", "load mispeculation due to replace",
					 &cpus[i].sim_load_mispec_rep, 0, NULL);
	stat_reg_counter(cpus[i].sdb, "load_mispec_inv", "load mispeculation due to intervention",
					 &cpus[i].sim_load_mispec_inv, 0, NULL);
	stat_reg_counter(cpus[i].sdb, "load_store_contention", "load->store contention",
					 &cpus[i].sim_load_store_contention, 0, NULL);

	stat_reg_counter(cpus[i].sdb, "sim_meas_insn",
	  	   "total number of instructions measured",
		   &cpus[i].sim_meas_insn, cpus[i].sim_meas_insn, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_detail_insn",
		   "total number of instructions in detailed simulation",
		   &cpus[i].sim_detail_insn, cpus[i].sim_detail_insn, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_sample_size",
		   "sample size when sampling is enabled",
		   &cpus[i].sim_sample_size, cpus[i].sim_sample_size, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_sample_period",
		   "sampling period when sampling is enabled",
		   &cpus[i].sim_sample_period, cpus[i].sim_sample_period, NULL);

    stat_reg_counter(cpus[i].sdb, "sim_pop_insn",
		   "total number of instructions committed",
		   &cpus[i].sim_pop_insn, cpus[i].sim_pop_insn, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_num_refs",
		   "total number of loads and stores committed",
		   &cpus[i].sim_num_refs, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_num_loads",
		   "total number of loads committed",
		   &cpus[i].sim_num_loads, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_num_stores",
		   "total number of stores committed",
		   &cpus[i].sim_num_stores, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_num_branches",
		   "total number of branches committed",
		   &cpus[i].sim_num_branches, /* initial value */0, /* format */NULL);
    stat_reg_int(cpus[i].sdb, "sim_elapsed_time",
	       "total simulation time in seconds",
	       &sim_elapsed_time, 0, NULL);
    stat_reg_double(cpus[i].sdb, "sim_inst_rate",
		   "simulation speed (in measured insts/sec)",
		   &cpus[i].sim_inst_rate, 0, NULL);
    stat_reg_double(cpus[i].sdb, "sim_pop_rate",
		   "simulation speed (in all simulated insts/sec)",
		   &cpus[i].sim_pop_rate, 0,  NULL);
    stat_reg_counter(cpus[i].sdb, "sim_total_insn",
		   "total number of instructions executed",
		   &cpus[i].sim_total_insn, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_total_refs",
		   "total number of loads and stores executed",
		   &cpus[i].sim_total_refs, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_total_loads",
		   "total number of loads executed",
		   &cpus[i].sim_total_loads, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_misspec_load", 
           "miss speculation loads", 
	       &cpus[i].sim_missspec_load, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_total_stores",
		   "total number of stores executed",
		   &cpus[i].sim_total_stores, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_total_branches",
		   "total number of branches executed",
		   &cpus[i].sim_total_branches, /* initial value */0, /* format */NULL);

	/* register pipeline stats */
    stat_reg_counter(cpus[i].sdb, "sim_fetch_insn",
		   "total number of insn fetched",
		   &cpus[i].sim_fetch_insn, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_decode_insn",
		   "total number of insn decoded",
		   &cpus[i].sim_decode_insn, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_map_insn",
		   "total number of insn mapped",
		   &cpus[i].sim_map_insn, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_issue_insn",
		   "total number of insn issued",
		   &cpus[i].sim_issue_insn, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_writeback_insn",
		   "total number of insn writeback",
		   &cpus[i].sim_writeback_insn, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_commit_insn",
		   "total number of insn committed",
		   &cpus[i].sim_commit_insn, 0, NULL);

    /* register performance stats */
    stat_reg_counter(cpus[i].sdb, "sim_meas_cycle",
		   "total number of cycles measured",
		   &cpus[i].sim_meas_cycle, cpus[i].sim_meas_cycle, NULL);
    stat_reg_double(cpus[i].sdb, "sim_IPC",
		   "instructions per cycle",
		   &cpus[i].sim_IPC, 0, NULL);
    stat_reg_double(cpus[i].sdb, "total_IPC",
		   "instructions per cycle",
		   &cpus[i].total_IPC, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "total packets",
		   "total number of packets transfered",
		   &cpus[i].total_packets, 0, NULL);

    stat_reg_double(cpus[i].sdb, "sim_CPI",
		   "cycles per instruction",
		   &cpus[i].sim_CPI, 0, NULL);
    stat_reg_double(cpus[i].sdb, "cycle_sample_stdev",
		   "Standard Deviation of Cycles per Sampling Unit",
		   &cpus[i].standardDeviation, cpus[i].standardDeviation, /*format*/NULL);
    stat_reg_double(cpus[i].sdb, "CPI_conf_interval",
		   "99.7% Confidence +/- % CPI error ",
		   &cpus[i].percentageErrorInterval, cpus[i].percentageErrorInterval , NULL);
    stat_reg_int(cpus[i].sdb, "CPI_recommended_k",
		   "k which is likely to achieve 99.7% +/- 3% CPI error",
		   &cpus[i].recommendedK, cpus[i].recommendedK, NULL);

    stat_reg_double(cpus[i].sdb, "sim_exec_BW",
		   "total instructions (mis-spec + committed) per cycle",
		   &cpus[i].sim_exec_BW, 0, NULL);
    stat_reg_double(cpus[i].sdb, "sim_IPB",
		   "instruction per branch",
		   &cpus[i].sim_IPB, 0, NULL);

#if 0
  stat_reg_double(sdb, "pwr_sample_stdev",
		   "Standard Deviation of pwr per Sampling Unit",
		   &pwr_standardDeviation, pwr_standardDeviation, /* format */NULL);
  stat_reg_double(sdb, "pwr_conf_interval",
		   "99.7% Confidence +/- % avg power error ",
		   &pwr_percentageErrorInterval , pwr_percentageErrorInterval , NULL);
  stat_reg_int(sdb, "pwr_recommended_k",
		   "k which is likely to achieve 99.7% +/- 3% pwr error",
		   &pwr_recommendedK, pwr_recommendedK, NULL);
#endif
  
    /* occupancy stats */
    stat_reg_counter(cpus[i].sdb, "roq_count", "cumulative roq occupancy",
                   &cpus[i].roq_count, /* initial value */0, /* format */NULL);
    stat_reg_counter(cpus[i].sdb, "roq_fcount", "cumulative roq full count",
                   &cpus[i].roq_fcount, /* initial value */0, /* format */NULL);
    stat_reg_double(cpus[i].sdb, "roq_occupancy", "avg roq occupancy (insn's)",
                   &cpus[i].roq_occupancy, 0, NULL);
    stat_reg_double(cpus[i].sdb, "roq_rate","avg roq dispatch rate (insn/cycle)",
                   &cpus[i].roq_rate, 0, NULL);
    stat_reg_double(cpus[i].sdb, "roq_latency", "avg roq occupant latency (cycle's)",
                   &cpus[i].roq_latency, 0, NULL);
    stat_reg_double(cpus[i].sdb, "roq_full", "fraction of time (cycle's) roq was full",
                   &cpus[i].roq_full, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "brq_count", "cumulative brq occupancy",
                   &cpus[i].brq_count, /* initial value */0, /* format */NULL);
    stat_reg_counter(cpus[i].sdb, "brq_fcount", "cumulative brq full count",
                   &cpus[i].brq_fcount, /* initial value */0, /* format */NULL);
    stat_reg_double(cpus[i].sdb, "brq_occupancy", "avg brq occupancy (insn's)",
                   &cpus[i].brq_occupancy, 0, NULL);
    stat_reg_double(cpus[i].sdb, "brq_rate", "avg brq dispatch rate (insn/cycle)",
                   &cpus[i].brq_rate, 0, NULL);
    stat_reg_double(cpus[i].sdb, "brq_latency", "avg brq occupant latency (cycle's)",
                   &cpus[i].brq_latency, 0, NULL);
    stat_reg_double(cpus[i].sdb, "brq_full", "fraction of time (cycle's) brq was full",
                   &cpus[i].brq_full, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "lsq_count", "cumulative lsq occupancy",
                   &cpus[i].lsq_count, /* initial value */0, /* format */NULL);
    stat_reg_counter(cpus[i].sdb, "lsq_fcount", "cumulative lsq full count",
                   &cpus[i].lsq_fcount, /* initial value */0, /* format */NULL);
    stat_reg_double(cpus[i].sdb, "lsq_occupancy", "avg lsq occupancy (insn's)",
                   &cpus[i].lsq_occupancy, 0, NULL);
    stat_reg_double(cpus[i].sdb, "lsq_rate","avg lsq dispatch rate (insn/cycle)",
                   &cpus[i].lsq_rate, 0, NULL);
    stat_reg_double(cpus[i].sdb, "lsq_latency", "avg lsq occupant latency (cycle's)",
                   &cpus[i].lsq_latency, 0, NULL);
    stat_reg_double(cpus[i].sdb, "lsq_full", "fraction of time (cycle's) lsq was full",
                   &cpus[i].lsq_full, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "intq_count", "cumulative intq occupancy",
                   &cpus[i].intq_count, /* initial value */0, /* format */NULL);
    stat_reg_counter(cpus[i].sdb, "intq_fcount", "cumulative intq full count",
                   &cpus[i].intq_fcount, /* initial value */0, /* format */NULL);
    stat_reg_double(cpus[i].sdb, "intq_occupancy", "avg intq occupancy (insn's)",
                   &cpus[i].intq_occupancy, 0, NULL);
    stat_reg_double(cpus[i].sdb, "intq_rate", "avg intq dispatch rate (insn/cycle)",
                   &cpus[i].intq_rate, 0, NULL);
    stat_reg_double(cpus[i].sdb, "intq_latency", "avg intq occupant latency (cycle's)",
                   &cpus[i].intq_latency, 0, NULL);
    stat_reg_double(cpus[i].sdb, "intq_full", "fraction of time (cycle's) intq was full",
                   &cpus[i].intq_full, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "fpq_count", "cumulative fpq occupancy",
                   &cpus[i].fpq_count, /* initial value */0, /* format */NULL);
    stat_reg_counter(cpus[i].sdb, "fpq_fcount", "cumulative fpq full count",
                   &cpus[i].fpq_fcount, /* initial value */0, /* format */NULL);
    stat_reg_double(cpus[i].sdb, "fpq_occupancy", "avg fpq occupancy (insn's)",
                   &cpus[i].fpq_occupancy, 0, NULL);
    stat_reg_double(cpus[i].sdb, "fpq_rate", "avg fpq dispatch rate (insn/cycle)",
                   &cpus[i].fpq_rate, 0, NULL);
    stat_reg_double(cpus[i].sdb, "fpq_latency", "avg fpq occupant latency (cycle's)",
                   &cpus[i].fpq_latency, 0, NULL);
    stat_reg_double(cpus[i].sdb, "fpq_full", "fraction of time (cycle's) fpq was full",
                   &cpus[i].fpq_full, 0, NULL);

# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
    stat_reg_counter(cpus[i].sdb, "sim_slip",
                   "total number of slip cycles",
                   &cpus[i].sim_slip, 0, NULL);
    /* register baseline stats */
    stat_reg_double(cpus[i].sdb, "avg_sim_slip",
                   "the average slip between issue and retirement",
                   &cpus[i].avg_sim_slip, 0, NULL);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  /* register predictor stats */
    if (cpus[i].pred)
      bpred_reg_stats(cpus[i].pred, cpus[i].sdb);

    stat_reg_counter(cpus[i].sdb, "sim_brbus_count",
		   "total number of branch bus valid",
		   &cpus[i].sim_brbus_count, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_brerr_count",
		   "total number of branch bus error",
		   &cpus[i].sim_brerr_count, 0, NULL);
    stat_reg_double(cpus[i].sdb, "sim_br_miss",
		   "branch miss rate",
		   &cpus[i].sim_br_miss, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "sim_bht_count",
		   "total number of conditional non-likely branche",
		   &cpus[i].sim_bht_count, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_bhterr_count",
		   "total number of bht branch bus error",
		   &cpus[i].sim_bhterr_count, 0, NULL);
    stat_reg_double(cpus[i].sdb, "sim_bht_miss",
		   "bht branch miss rate",
		   &cpus[i].sim_bht_miss, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "sim_jr_count",
		   "total number of jr branch bus valid",
		   &cpus[i].sim_jr_count, 0, NULL);
    stat_reg_counter(cpus[i].sdb, "sim_jrerr_count",
		   "total number of jr branch bus error",
		   &cpus[i].sim_jrerr_count, 0, NULL);
    stat_reg_double(cpus[i].sdb, "sim_jr_miss",
		   "jr branch miss rate",
		   &cpus[i].sim_jr_miss, 0, NULL);
  
    stat_reg_counter(cpus[i].sdb, "dcache_read_low_level_count",
		   "total number of accepted dcache miss read request",
		   &cpus[i].dcache_read_low_level_count, /* initial value */0, /* format */NULL);

    stat_reg_double(cpus[i].sdb, "dcache_miss",
		   "dcache miss per k committed instruction",
		   &cpus[i].dcache_miss, 0, NULL);

    stat_reg_double(cpus[i].sdb, "dcache_miss_rate",
		   "dcache miss rate",
		   &cpus[i].dcache_miss_rate, 0, NULL);
  
    stat_reg_counter(cpus[i].sdb, "icache_read_low_level_count",
		   "total number of accepted icache miss read request",
		   &cpus[i].icache_read_low_level_count, /* initial value */0, /* format */NULL);

    stat_reg_double(cpus[i].sdb, "icache_miss",
		   "icache miss per k committed instruction",
		   &cpus[i].icache_miss, 0, NULL);

    stat_reg_double(cpus[i].sdb, "icache_miss_rate",
		   "icache miss rate",
		   &cpus[i].icache_miss_rate, 0, NULL);
  
    stat_reg_counter(cpus[i].sdb, "noc_read_count",
		   "total number of network on chip read request",
		   &cpus[i].noc_read_count, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "noc_write_count",
		   "total number of network on chip write request",
		   &cpus[i].noc_write_count, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "memq_read_count",
		   "total number of memory queue read request",
		   &cpus[i].memq_read_count, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "memq_write_count",
		   "total number of memory queue write request",
		   &cpus[i].memq_write_count, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "memq_busy_count",
		   "total number of memory queue busy cycle",
		   &cpus[i].memq_busy_count, /* initial value */0, /* format */NULL);

    stat_reg_double(cpus[i].sdb, "memq_busy_rate",
		   "time percent of memory queue busy",
		   &cpus[i].memq_busy_rate, 0, NULL);

    stat_reg_counter(cpus[i].sdb, "sim_loadcnt",
		   "total number of loads",
		   &cpus[i].sim_loadcnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "sim_loadmisscnt",
		   "total number of load miss",
		   &cpus[i].sim_loadmisscnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "sim_storecnt",
		   "total number of stores",
		   &cpus[i].sim_storecnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "sim_storemisscnt",
		   "total number of miss store",
		   &cpus[i].sim_storemisscnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "sim_loadfwdcnt",
		   "total number of loads that forward value from ealier stores",
		   &cpus[i].sim_loadfwdcnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "sim_storefwdcnt",
		   "total number of stores that forward value to loads",
		   &cpus[i].sim_storefwdcnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "sim_ldwtbkdelaycnt",
		   "total number of writeback-delayed loads",
		   &cpus[i].sim_ldwtbkdelaycnt, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_accesses",
		   "total number of L1 dcache accesses",
		   &cpus[i].L1_accesses, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_stores",
		   "total number of L1 dcache stores",
		   &cpus[i].L1_stores, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_hits",
		   "total number of L1 dcache hits",
		   &cpus[i].L1_hits, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_misses",
		   "total number of L1 dcache misses",
		   &cpus[i].L1_misses, /* initial value */0, /* format */NULL);

    stat_reg_double(cpus[i].sdb, "L1_miss_rate",
		   "total number of L1 dcache miss rate",
		   &cpus[i].L1_miss_rate, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_writebacks",
		   "total number of L1 dcache writebacks",
		   &cpus[i].L1_writebacks, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_exclusive_to_modified_changes",
		   "total number of L1 cache exclusive to modified changes",
		   &cpus[i].L1_exclusive_to_modified_changes, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_read_shared_requests",
		   "total number of L1 cacheread shared requests",
		   &cpus[i].L1_read_shared_requests, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_read_exclusive_requests",
		   "total number of L1 cache read exclusive requests",
		   &cpus[i].L1_read_exclusive_requests, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_upgrade_requests",
		   "total number of L1 cache upgrade requests",
		   &cpus[i].L1_upgrade_requests, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_external_invalidations",
		   "total number of L1 cache external invalidations",
		   &cpus[i].L1_external_invalidations, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L1_external_intervention_shareds",
		   "total number of L1 cache external intervention shareds",
		   &cpus[i].L1_external_intervention_shareds, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L2_hits",
		   "total number of L2 cache hits",
		   &cpus[i].L2_hits, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L2_misses",
		   "total number of L2 cache misses",
		   &cpus[i].L2_misses, /* initial value */0, /* format */NULL);

    stat_reg_double(cpus[i].sdb, "L2_miss_rate",
		   "L2 cache miss rate",
		   &cpus[i].L2_miss_rate, /* initial value */0, /* format */NULL);

    stat_reg_counter(cpus[i].sdb, "L2_writebacks",
		   "total number of L2 cache writebacks",
		   &cpus[i].L2_writebacks, /* initial value */0, /* format */NULL);

#if 0
  stat_reg_counter(sdb, "wb_load_replaytrap",
		   "total number of load replay traps",
		   &wb_load_replaytrap, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "cache_quadword_trap",
		   "traps due to missing loads to same address",
		   &cache_quadword_trap, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "cache_diffaddr_trap",
		   "traps due to diff addr. mappping to same line",
		   &cache_diffaddr_trap, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "wb_store_replaytrap",
		   "total number of store replay traps",
		   &wb_store_replaytrap, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "wb_diffsize_replaytrap",
		   "total number of different size  replay traps",
		   &wb_diffsize_replaytrap, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "commit_ctrl_flushes",
		   "total number of control pipeline flushes",
		   &commit_ctrl_flushes, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "commit_trap_flushes",
		   "total number of trap pipeline flushes",
		   &commit_trap_flushes, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "map_num_early_retire",
		   "Number of instructions which retired early",
		   &map_num_early_retire, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "victim buffer hits",
		   "Number of Victim buffer hits",
		   &victim_buf_hits, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "victim buffer misses",
		   "Number of victim buffer misses",
		   &victim_buf_misses, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "syscall_cycles",
		   "Number of cycles lost due to syscalls",
		   &sys_cycles_lost, /* initial value */0, /* format */NULL);
  stat_reg_counter(sdb, "trap_cycles",
		   "Number of cycles lost due to traps",
		   &wb_trap_cycles_lost, /* initial value */0, /* format */NULL);
#endif

#if 0
  /* register cache stats */
  for (i=0; i<num_caches; i++)
    cache_timing_reg_stats(caches[i], sdb);
  
  for (i=0; i<num_tlbs; i++)
    cache_timing_reg_stats(tlbs[i], sdb);

  for (i=0; i<num_buses; i++)
    bus_reg_stats(buses[i], sdb);

  for (i=0; i<num_mem_banks; i++)
    mem_bank_reg_stats(mem_banks[i], sdb);
#endif
  }  //end of cpu iteration

  /* register power stats */
#ifdef POWER_STAT
  power_reg_stats(sdb);
#endif
	
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  for (i=0; i<pcstat_nelt; i++) {
    char buf[512], buf1[512];
    struct stat_stat_t *stat;

    /* track the named statistical variable by text address */

    /* find it... */
    stat = stat_find_stat(sdb, pcstat_vars[i]);
    if (!stat)
      fatal("cannot locate any statistic named `%s'", pcstat_vars[i]);

    /* stat must be an integral type */
    if (stat->sc != sc_int && stat->sc != sc_uint && stat->sc != sc_counter)
      fatal("`-pcstat' statistical variable `%s' is not an integral type",
	  stat->name);

    /* register this stat */
    pcstat_stats[i] = stat;
    pcstat_lastvals[i] = STATVAL(stat);

    /* declare the sparce text distribution */
    sprintf(buf, "%s_by_pc", stat->name);
    sprintf(buf1, "%s (by text address)", stat->desc);
    pcstat_sdists[i] = stat_reg_sdist(sdb, buf, buf1,
	/* initial value */0,
	/* print format */(PF_COUNT|PF_PDF),
	/* format */"0x%lx %lu %.2f",
	/* print fn */NULL);
  }
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  
  ld_reg_stats(sdb);
  mem_reg_stats(sdb);
  //ss_mem_reg_stats(mem,sdb);
}

/* compute the formula which has been registerd */
void
compute_formula()
{
  int i, j;
  counter_t total_packets = 0, total_latency = 0,total_insn=0,total_cycle=0;
  for(i=0 ; i<total_cpus ; i++){
	cpus[i].sim_num_stores = cpus[i].sim_num_refs - cpus[i].sim_num_loads;
    cpus[i].sim_total_stores = cpus[i].sim_total_refs - cpus[i].sim_total_loads;
	
    cpus[i].sim_inst_rate = (double)cpus[i].sim_meas_insn / sim_elapsed_time;
    cpus[i].sim_pop_rate = (double)cpus[i].sim_pop_insn / sim_elapsed_time;
	
    cpus[i].sim_IPC = (double)cpus[i].sim_meas_insn / cpus[i].sim_meas_cycle;
    total_insn += cpus[i].sim_meas_insn;
    total_cycle += cpus[i].sim_meas_cycle;
    cpus[i].sim_CPI = (double)cpus[i].sim_meas_cycle / cpus[i].sim_meas_insn;
    cpus[i].sim_exec_BW = (double)cpus[i].sim_total_insn / cpus[i].sim_meas_cycle;
    cpus[i].sim_IPB = (double)cpus[i].sim_meas_insn / cpus[i].sim_num_branches;
	
    cpus[i].roq_occupancy = (double)cpus[i].roq_count / cpus[i].sim_meas_cycle;
    cpus[i].roq_rate = (double)cpus[i].sim_total_insn / cpus[i].sim_meas_cycle;
    cpus[i].roq_latency = (double)cpus[i].roq_occupancy / cpus[i].roq_rate;
    cpus[i].roq_full = (double)cpus[i].roq_fcount / cpus[i].sim_meas_cycle;
	
    cpus[i].brq_occupancy = (double)cpus[i].brq_count / cpus[i].sim_meas_cycle;
    cpus[i].brq_rate = (double)cpus[i].sim_total_insn / cpus[i].sim_meas_cycle;
    cpus[i].brq_latency = (double)cpus[i].brq_occupancy / cpus[i].brq_rate;
    cpus[i].brq_full = (double)cpus[i].brq_fcount / cpus[i].sim_meas_cycle;
	
    cpus[i].lsq_occupancy = (double)cpus[i].lsq_count / cpus[i].sim_meas_cycle;
    cpus[i].lsq_rate = (double)cpus[i].sim_total_insn / cpus[i].sim_meas_cycle;
    cpus[i].lsq_latency = (double)cpus[i].lsq_occupancy / cpus[i].lsq_rate;
    cpus[i].lsq_full = (double)cpus[i].lsq_fcount / cpus[i].sim_meas_cycle;
	
    cpus[i].intq_occupancy = (double)cpus[i].intq_count / cpus[i].sim_meas_cycle;
    cpus[i].intq_rate = (double)cpus[i].sim_total_insn / cpus[i].sim_meas_cycle;
    cpus[i].intq_latency = (double)cpus[i].intq_occupancy / cpus[i].intq_rate;
    cpus[i].intq_full = (double)cpus[i].intq_fcount / cpus[i].sim_meas_cycle;
	
    cpus[i].fpq_occupancy = (double)cpus[i].fpq_count / cpus[i].sim_meas_cycle;
    cpus[i].fpq_rate = (double)cpus[i].sim_total_insn / cpus[i].sim_meas_cycle;
    cpus[i].fpq_latency = (double)cpus[i].fpq_occupancy / cpus[i].fpq_rate;
    cpus[i].fpq_full = (double)cpus[i].fpq_fcount / cpus[i].sim_meas_cycle;
	
    cpus[i].avg_sim_slip = (double)cpus[i].sim_slip / cpus[i].sim_meas_insn;
	
    cpus[i].sim_br_miss = (double)cpus[i].sim_brerr_count / cpus[i].sim_brbus_count;
    cpus[i].sim_bht_miss = (double)cpus[i].sim_bhterr_count / cpus[i].sim_bht_count;
    cpus[i].sim_jr_miss = (double)cpus[i].sim_jrerr_count / cpus[i].sim_jr_count;
  
    cpus[i].dcache_miss = 1000.0 * (double)cpus[i].dcache_read_low_level_count / cpus[i].sim_meas_insn;
    cpus[i].dcache_miss_rate = 100.0 * (double)cpus[i].dcache_read_low_level_count / cpus[i].sim_total_refs;
    cpus[i].icache_miss = 1000.0 * (double)cpus[i].icache_read_low_level_count / cpus[i].sim_meas_insn;
    cpus[i].icache_miss_rate = 100.0 * (double)cpus[i].icache_read_low_level_count / cpus[i].sim_meas_insn;
    cpus[i].memq_busy_rate = (double)cpus[i].memq_busy_count / cpus[i].sim_meas_cycle;
    cpus[i].L1_miss_rate = 100.0 * (double)cpus[i].L1_misses / ( cpus[i].L1_misses + cpus[i].L1_hits );
    cpus[i].L2_miss_rate = 100.0 * (double)cpus[i].L2_misses / ( cpus[i].L2_misses + cpus[i].L2_hits );

#ifdef POWER_STAT
    /* power statistics */
    if(!gated_clock) {
      cpus[i].bpred_power->clock = cpus[i].bpred_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].decode_power->clock = cpus[i].decode_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].rename_power->clock = cpus[i].rename_power->clock * cpus[i].sim_meas_cycle; 
      cpus[i].window_power->clock = cpus[i].window_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].ialu_power->clock = cpus[i].ialu_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].falu_power->clock = cpus[i].falu_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].lsq_power->clock = cpus[i].lsq_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].roq_power->clock = cpus[i].roq_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].regfile_power->clock = cpus[i].regfile_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].icache_power->clock = cpus[i].icache_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].dcache_power->clock = cpus[i].dcache_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].itlb_power->clock = cpus[i].itlb_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].dtlb_power->clock = cpus[i].dtlb_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].cache_dl2_power->clock = cpus[i].cache_dl2_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].cache2mem_power->clock = cpus[i].cache2mem_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].router_power->clock = cpus[i].router_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].IOpad_power->clock = cpus[i].IOpad_power->clock * cpus[i].sim_meas_cycle;
      cpus[i].clocktree_power->clock = cpus[i].clocktree_power->clock * cpus[i].sim_meas_cycle;
 
      for(j=0 ; j<NUM_TYPES ; j++){
        cpus[i].input_buffer_power[j]->clock = cpus[i].input_buffer_power[j]->clock * cpus[i].sim_meas_cycle;
        cpus[i].arbiter_power[j]->clock = cpus[i].arbiter_power[j]->clock * cpus[i].sim_meas_cycle;
        cpus[i].crossbar_power[j]->clock = cpus[i].crossbar_power[j]->clock * cpus[i].sim_meas_cycle;
        cpus[i].link_power[j]->clock = cpus[i].link_power[j]->clock * cpus[i].sim_meas_cycle;
      }
    }

    cpus[i].cpu_power->clock = cpus[i].bpred_power->clock + cpus[i].decode_power->clock
                             + cpus[i].rename_power->clock + cpus[i].window_power->clock
                             + cpus[i].ialu_power->clock + cpus[i].lsq_power->clock
                             + cpus[i].falu_power->clock
                             + cpus[i].roq_power->clock + cpus[i].regfile_power->clock
                             + cpus[i].icache_power->clock + cpus[i].dcache_power->clock
                             + cpus[i].itlb_power->clock + cpus[i].dtlb_power->clock
                             + cpus[i].cache_dl2_power->clock + cpus[i].cache2mem_power->clock
                             + cpus[i].router_power->clock + cpus[i].IOpad_power->clock
                             + cpus[i].clocktree_power->clock;

    cpus[i].bpred_power->total = cpus[i].bpred_power->dynamic + cpus[i].bpred_power->clock + cpus[i].bpred_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].decode_power->total = cpus[i].decode_power->dynamic + cpus[i].decode_power->clock + cpus[i].decode_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].rename_power->total = cpus[i].rename_power->dynamic + cpus[i].rename_power->clock + cpus[i].rename_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].window_power->total = cpus[i].window_power->dynamic + cpus[i].window_power->clock + cpus[i].window_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].ialu_power->total = cpus[i].ialu_power->dynamic + cpus[i].ialu_power->clock + cpus[i].ialu_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].falu_power->total = cpus[i].falu_power->dynamic + cpus[i].falu_power->clock + cpus[i].falu_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].lsq_power->total = cpus[i].lsq_power->dynamic + cpus[i].lsq_power->clock + cpus[i].lsq_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].roq_power->total = cpus[i].roq_power->dynamic + cpus[i].roq_power->clock + cpus[i].roq_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].regfile_power->total = cpus[i].regfile_power->dynamic + cpus[i].regfile_power->clock + cpus[i].regfile_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].icache_power->total = cpus[i].icache_power->dynamic + cpus[i].icache_power->clock + cpus[i].icache_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].dcache_power->total = cpus[i].dcache_power->dynamic + cpus[i].dcache_power->clock + cpus[i].dcache_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].itlb_power->total = cpus[i].itlb_power->dynamic + cpus[i].itlb_power->clock + cpus[i].itlb_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].dtlb_power->total = cpus[i].dtlb_power->dynamic + cpus[i].dtlb_power->clock + cpus[i].dtlb_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].cache_dl2_power->total = cpus[i].cache_dl2_power->dynamic + cpus[i].cache_dl2_power->clock + cpus[i].cache_dl2_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].cache2mem_power->total = cpus[i].cache2mem_power->dynamic + cpus[i].cache2mem_power->clock + cpus[i].cache2mem_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].router_power->total = cpus[i].router_power->dynamic + cpus[i].router_power->clock + cpus[i].router_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].IOpad_power->total = cpus[i].IOpad_power->dynamic + cpus[i].IOpad_power->clock + cpus[i].IOpad_power->leakage * cpus[i].sim_meas_cycle;
    cpus[i].clocktree_power->total = cpus[i].clocktree_power->dynamic + cpus[i].clocktree_power->clock + cpus[i].clocktree_power->leakage * cpus[i].sim_meas_cycle;

    for(j=0 ; j<NUM_TYPES ; j++){
      cpus[i].input_buffer_power[j]->total = cpus[i].input_buffer_power[j]->dynamic + cpus[i].input_buffer_power[j]->clock + cpus[i].input_buffer_power[j]->leakage* cpus[i].sim_meas_cycle;
      cpus[i].arbiter_power[j]->total = cpus[i].arbiter_power[j]->dynamic + cpus[i].arbiter_power[j]->clock + cpus[i].arbiter_power[j]->leakage* cpus[i].sim_meas_cycle;
      cpus[i].crossbar_power[j]->total = cpus[i].crossbar_power[j]->dynamic + cpus[i].crossbar_power[j]->clock + cpus[i].crossbar_power[j]->leakage* cpus[i].sim_meas_cycle;
      cpus[i].link_power[j]->total = cpus[i].link_power[j]->dynamic + cpus[i].link_power[j]->clock + cpus[i].link_power[j]->leakage* cpus[i].sim_meas_cycle;
    }

    cpus[i].cpu_power->total = cpus[i].bpred_power->total + cpus[i].decode_power->total
                             + cpus[i].rename_power->total + cpus[i].window_power->total
                             + cpus[i].ialu_power->total + cpus[i].lsq_power->total
                             + cpus[i].falu_power->total
                             + cpus[i].roq_power->total + cpus[i].regfile_power->total
                             + cpus[i].icache_power->total + cpus[i].dcache_power->total
                             + cpus[i].itlb_power->total + cpus[i].dtlb_power->total
                             + cpus[i].cache_dl2_power->total + cpus[i].cache2mem_power->total
                             + cpus[i].router_power->total + cpus[i].IOpad_power->total
                             + cpus[i].clocktree_power->total;

    cpus[i].avg_bpred_power_total = cpus[i].bpred_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_decode_power_total = cpus[i].decode_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_rename_power_total = cpus[i].rename_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_window_power_total = cpus[i].window_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_ialu_power_total = cpus[i].ialu_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_falu_power_total = cpus[i].falu_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_lsq_power_total = cpus[i].lsq_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_roq_power_total = cpus[i].roq_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_regfile_power_total = cpus[i].regfile_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_icache_power_total = cpus[i].icache_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_dcache_power_total = cpus[i].dcache_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_itlb_power_total = cpus[i].itlb_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_dtlb_power_total = cpus[i].dtlb_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_cache_dl2_power_total = cpus[i].cache_dl2_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_cache2mem_power_total = cpus[i].cache2mem_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_router_power_total = cpus[i].router_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_IOpad_power_total = cpus[i].IOpad_power->total / cpus[i].sim_meas_cycle;
    cpus[i].avg_clocktree_power_total = cpus[i].clocktree_power->total / cpus[i].sim_meas_cycle;

    for(j=0 ; j<NUM_TYPES ; j++){
      cpus[i].avg_input_buffer_power_total[j] = cpus[i].input_buffer_power[j]->total / cpus[i].sim_meas_cycle;          cpus[i].avg_arbiter_power_total[j] = cpus[i].arbiter_power[j]->total / cpus[i].sim_meas_cycle;
      cpus[i].avg_crossbar_power_total[j] = cpus[i].crossbar_power[j]->total / cpus[i].sim_meas_cycle;
      cpus[i].avg_link_power_total[j] = cpus[i].link_power[j]->total / cpus[i].sim_meas_cycle;
    }

    cpus[i].avg_cpu_power_total = cpus[i].avg_bpred_power_total + cpus[i].avg_decode_power_total
                             + cpus[i].avg_rename_power_total + cpus[i].avg_window_power_total
                             + cpus[i].avg_ialu_power_total + cpus[i].avg_lsq_power_total
                             + cpus[i].avg_falu_power_total
                             + cpus[i].avg_roq_power_total + cpus[i].avg_regfile_power_total
                             + cpus[i].avg_icache_power_total + cpus[i].avg_dcache_power_total
                             + cpus[i].avg_itlb_power_total + cpus[i].avg_dtlb_power_total
                             + cpus[i].avg_cache_dl2_power_total + cpus[i].avg_cache2mem_power_total
                             + cpus[i].avg_router_power_total + cpus[i].avg_IOpad_power_total
                             + cpus[i].avg_clocktree_power_total;

    cpus[i].avg_bpred_power_dynamic = cpus[i].bpred_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_decode_power_dynamic = cpus[i].decode_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_rename_power_dynamic = cpus[i].rename_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_window_power_dynamic = cpus[i].window_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_ialu_power_dynamic = cpus[i].ialu_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_falu_power_dynamic = cpus[i].falu_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_lsq_power_dynamic = cpus[i].lsq_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_roq_power_dynamic = cpus[i].roq_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_regfile_power_dynamic = cpus[i].regfile_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_icache_power_dynamic = cpus[i].icache_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_dcache_power_dynamic = cpus[i].dcache_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_itlb_power_dynamic = cpus[i].itlb_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_dtlb_power_dynamic = cpus[i].dtlb_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_cache_dl2_power_dynamic = cpus[i].cache_dl2_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_cache2mem_power_dynamic = cpus[i].cache2mem_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_router_power_dynamic = cpus[i].router_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_IOpad_power_dynamic = cpus[i].IOpad_power->dynamic / cpus[i].sim_meas_cycle;
    cpus[i].avg_clocktree_power_dynamic = cpus[i].clocktree_power->dynamic / cpus[i].sim_meas_cycle;

    for(j=0 ; j<NUM_TYPES ; j++){
      cpus[i].avg_input_buffer_power_dynamic[j] = cpus[i].input_buffer_power[j]->dynamic / cpus[i].sim_meas_cycle;
      cpus[i].avg_arbiter_power_dynamic[j] = cpus[i].arbiter_power[j]->dynamic / cpus[i].sim_meas_cycle;
      cpus[i].avg_crossbar_power_dynamic[j] = cpus[i].crossbar_power[j]->dynamic / cpus[i].sim_meas_cycle;
      cpus[i].avg_link_power_dynamic[j] = cpus[i].link_power[j]->dynamic / cpus[i].sim_meas_cycle;
    }

    cpus[i].avg_cpu_power_dynamic = cpus[i].avg_bpred_power_dynamic + cpus[i].avg_decode_power_dynamic
                             + cpus[i].avg_rename_power_dynamic + cpus[i].avg_window_power_dynamic
                             + cpus[i].avg_ialu_power_dynamic
                             + cpus[i].avg_falu_power_dynamic + cpus[i].avg_lsq_power_dynamic
                             + cpus[i].avg_roq_power_dynamic + cpus[i].avg_regfile_power_dynamic
                             + cpus[i].avg_icache_power_dynamic + cpus[i].avg_dcache_power_dynamic
                             + cpus[i].avg_itlb_power_dynamic + cpus[i].avg_dtlb_power_dynamic
                             + cpus[i].avg_cache_dl2_power_dynamic + cpus[i].avg_cache2mem_power_dynamic
                             + cpus[i].avg_router_power_dynamic + cpus[i].avg_IOpad_power_dynamic
                             + cpus[i].avg_clocktree_power_dynamic;

    cpus[i].avg_bpred_power_clock = cpus[i].bpred_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_decode_power_clock = cpus[i].decode_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_rename_power_clock = cpus[i].rename_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_window_power_clock = cpus[i].window_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_ialu_power_clock = cpus[i].ialu_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_falu_power_clock = cpus[i].falu_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_lsq_power_clock = cpus[i].lsq_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_roq_power_clock = cpus[i].roq_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_regfile_power_clock = cpus[i].regfile_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_icache_power_clock = cpus[i].icache_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_dcache_power_clock = cpus[i].dcache_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_itlb_power_clock = cpus[i].itlb_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_dtlb_power_clock = cpus[i].dtlb_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_cache_dl2_power_clock = cpus[i].cache_dl2_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_cache2mem_power_clock = cpus[i].cache2mem_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_router_power_clock = cpus[i].router_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_IOpad_power_clock = cpus[i].IOpad_power->clock / cpus[i].sim_meas_cycle;
    cpus[i].avg_clocktree_power_clock = cpus[i].clocktree_power->clock / cpus[i].sim_meas_cycle;

    for(j=0 ; j<NUM_TYPES ; j++){
      cpus[i].avg_input_buffer_power_clock[j] = cpus[i].input_buffer_power[j]->clock / cpus[i].sim_meas_cycle;          cpus[i].avg_arbiter_power_clock[j] = cpus[i].arbiter_power[j]->clock / cpus[i].sim_meas_cycle;
      cpus[i].avg_crossbar_power_clock[j] = cpus[i].crossbar_power[j]->clock / cpus[i].sim_meas_cycle;
      cpus[i].avg_link_power_clock[j] = cpus[i].link_power[j]->clock / cpus[i].sim_meas_cycle;
    }

    cpus[i].avg_cpu_power_clock = cpus[i].avg_bpred_power_clock + cpus[i].avg_decode_power_clock
                             + cpus[i].avg_rename_power_clock + cpus[i].avg_window_power_clock
                             + cpus[i].avg_ialu_power_clock
                             + cpus[i].avg_falu_power_clock + cpus[i].avg_lsq_power_clock
                             + cpus[i].avg_roq_power_clock + cpus[i].avg_regfile_power_clock
                             + cpus[i].avg_icache_power_clock + cpus[i].avg_dcache_power_clock
                             + cpus[i].avg_itlb_power_clock + cpus[i].avg_dtlb_power_clock
                             + cpus[i].avg_cache_dl2_power_clock + cpus[i].avg_cache2mem_power_clock
                             + cpus[i].avg_router_power_clock + cpus[i].avg_IOpad_power_clock
                             + cpus[i].avg_clocktree_power_clock;

#endif

    for (j = 0; j < NUM_TYPES; j++) {
      cpus[i].router->avg_pack_latency[j] = (double)cpus[i].router->pack_latency[j] / cpus[i].router->pack_sent[j];
      total_latency += cpus[i].router->pack_latency[j];
      total_packets += cpus[i].router->pack_sent[j];
    }
  }

  for (i = 0; i < total_cpus; i++){
    cpus[i].router->avg_latency = (double)total_latency / total_packets;
    cpus[i].total_IPC = (double)total_insn/total_cycle;
    cpus[i].total_packets = total_packets;
  }
}

/* initialize the simulator */
void
sim_init(void)
{
  int i;
  for (i=0;i<total_cpus;i++) {
    regs_init(&cpus[i].regs);
    cpus[i].mem = mem_create("mem");
    ss_mem_init(cpus[i].mem);
    //calculate_power(&cpus[i].power);
  }
  //dump_power_stats(&cpus[0].power);
  
  dram_init();
  noc_init();
#ifdef POWER_STAT
  for (i=0;i<total_cpus;i++) {
        power_init(&cpus[i]);
  }
#endif
  //leakage_init();
}

/* multi program variables definition */
int  multiprog_num = 0;
char *mpargv[MAX_CPUS][MAX_ARGC];
int  mpargc[MAX_CPUS];
char **mpenvp[MAX_CPUS];

/* check and get multi program arguments */
void
check_multiprog_args(char *colon_num, int myargc, char **myargv, char **myenvp)
{
  if (colon_num[0] != ':')
	panic("something wrong happen while check multiprog num!\n");

  multiprog_num = atoi(&colon_num[1]);

  if (multiprog_num > total_cpus)
	panic("multiprog num cannot be more than total cpu num!\n");

  if (multiprog_num != myargc)
	panic("multiprog num should be equal to number of arguments!\n");
  
  int  i=0, j=0, m=0, n=0;

  for (i=0 ; i<MAX_CPUS ; i++){
    for (j=0 ; j<MAX_ARGC ; j++)
      mpargv[i][j] = (char *) malloc(sizeof(char) * MAX_STRING_LENGTH);
  }

  /* now parse the input arguments */
  char *p;
  for (i=0 ; i<myargc ; i++) {
	p=myargv[i]; j=0; m=0; n=0;
	while (*(p+m)!='\0' && *(p+n)!='\0') {
      while (*(p+m) == ' ')  m++;
	  while (*(p+n)!=' ' && *(p+n)!='\0')  n++;

	  if (m != n) {
	    memcpy(mpargv[i][j], p+m, n-m);
		mpargv[i][j][n-m] = '\0';
	    mpargc[i] = ++j;
	    m = n++;
	  }
	}

	mpenvp[i] = myenvp;
  }

  /* check for redirect operation to stdin*/
  int index;
  for (i=0 ; i<myargc ; i++) {
    index = mpargc[i]-2;
	if (!strcmp(mpargv[i][index],"<")) {
	  mpargc[i] -= 2;
	  cpus[i].stdin_addr = mpargv[i][index+1];
	}
  }

}

/* load multi program into simulated state */
void
sim_load_multiprog()
{
  int i;
  for(i=0 ; i<multiprog_num ; i++){
    /* load program text and data, set up environment, memory, and regs */
    ld_load_prog(mpargv[i][0], mpargc[i], mpargv[i], mpenvp[i], &cpus[i], &cpus[i].regs, cpus[i].mem, TRUE);

    mem_init1(i);

	cpus[i].active = 1;  /* mark the active cpu */
	
    //predecode_init(i);
  }

}

int is_splash = 0;

/* load splash program into simulated state */
void
sim_load_splash_prog(char *fname,
		  int argc, char **argv,
		  char **envp)
{
  //int i;
  //for(i=0 ; i<total_cpus ; i++){
    /* load program text and data, set up environment, memory, and regs */
    ld_load_prog(fname,argc,argv,envp,&cpus[0],&cpus[0].regs,cpus[0].mem,TRUE);
    
	mem_init1(0);
  
  //}
  
  cpus[0].active = 1;

  is_splash = 1;
}

/* load program into simulated state */
void
sim_load_prog(char *fname,		/* program to load */
	      int argc, char **argv,	/* program arguments */
	      char **envp)		/* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  ld_load_prog(fname,argc,argv,envp,&cpus[0],&cpus[0].regs,cpus[0].mem,TRUE);

# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  /* initialize here, so symbols can be loaded */
  if (ptrace_nelt == 2)
  {
    /* generate a pipeline trace */
    ptrace_open(/* fname */ptrace_opts[0], /* range */ptrace_opts[1]);
  }
  else if (ptrace_nelt == 0)
  {
    /* no pipetracing */;
  }
  else
    fatal("bad pipetrace args, use: <fname|stdout|stderr> <range>");
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

  cpus[0].active = 1;
  
  mem_init1(0);

  //predecode_init(0);

#ifdef ISTAT
  istat_init();
#endif
}

/* print simulator-specific configuration information */
void
sim_aux_config(FILE *stream)
{
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void
sim_aux_stats(FILE *stream)
{
  /* nada */
}

/* un-initialize simulator-specific state */
void
sim_uninit(void)
{
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  if (ptrace_nelt > 0)
    ptrace_close();
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
}

#if 0
/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void) {
  int n;
  int no_commit = 0;

  void exit_now(int);

  resource_init();

  commit_stage_init();

  writeback_stage_init();

  cache2mem_init();

  lsq_init();

  issue_stage_init();

  map_stage_init();

  decode_stage_init();

  fetch_stage_init();

  regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);

  /* Calculate overall sampling period if sampling is enabled */
  if (sampling_enabled) {
    sim_sample_period = sampling_k * sampling_munit;
  }

  /* Capture an initial backup of the empty simulation statistics */
  simoo_backup_stats();

  /* Until the simulation exits */
  while(1){
    int done_draining = 0;
    int cannot_stop = 0;
    counter_t end_of_sample = 0;
    counter_t start_of_measurement = 0;
    counter_t fast_forward_count = 0;

    /* calculate the number of instructions to fast forward. */
    if (opt_fastfwd_count && simulator_state == NOT_STARTED) {
      fast_forward_count = opt_fastfwd_count;
    } else if (sampling_enabled) {
      counter_t end_of_next_period =
	( ( (sim_pop_insn - opt_fastfwd_count) / sim_sample_period ) + 1 ) * sim_sample_period + opt_fastfwd_count;
      fast_forward_count = end_of_next_period - sim_pop_insn - sampling_wunit - sampling_munit;
    }

    /* Go to fast_forwarding mode */
    switch_to_fast_forwarding();

    if (fast_forward_count>0) {
      run_fast(fast_forward_count);
    }

    switch_to_warming();

    /* set up timing simulation entry state */
    fetch_reg_PC = regs.regs_PC;

    if (! sampling_enabled ) {
      /* Go straight to the measuring state if there is no sampling, or if warming is zero */
      start_measuring();
    } else {
      /* Calculate the end of the warming period for this warming interval */
      start_of_measurement = sim_pop_insn + sampling_wunit;

      /* Stop measureing m-unit instructions after we stop warming */
      end_of_sample = start_of_measurement + sampling_munit;
    }

    /* main simulator loop, NOTE: the pipe stages are traverse in reverse order
     *        to eliminate this/next state synchronization and relaxation problems */
    while (!done_draining) {
      /* Note: when sampling is off, done_draining can never become true, so
       *          sample && is implicitly part of this condition */
      if ((simulator_state == WARMING) && sim_pop_insn >= start_of_measurement) {
	start_measuring();
      } else if (sampling_enabled && (simulator_state == MEASURING) && sim_pop_insn >= end_of_sample) {
	stop_measuring();
      }

      if (((roq_head+roq_num) % roq_ifq_size) != roq_tail)
	panic ("roq_head/roq_tail wedged");

      if (((brq_head+brq_num) % brq_ifq_size) != brq_tail)
	panic ("brq_head/brq_tail wedged");

      /* Service memory hierarchy events */
      eventq_service_events(sim_cycle);

      //if (sim_cycle>100000) 
      //  roq_dump();

      /*run through the pipeline stages*/

#ifdef POWER_STAT     
      /* added for Wattch to clear hardware access counters */
      clear_access_stats();
#endif

#     if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
      /* check if pipetracing is still active */
      ptrace_check_active(regs.regs_PC, sim_pop_insn, sim_cycle);

      /* indicate new cycle in pipetrace */
      ptrace_newcycle(sim_cycle);
#     endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      /* call the commit stage */
      n = commit_stage();

#     if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
      if (n==0) {
	no_commit++;
	if (no_commit>3000) {
	  roq_dump();
	  lsq_dump();
	  cache2mem_dump();
	  fatal("more than 3000 cycle no commit,probably dead locked!\n");
	}
      }else{
	no_commit=0;
      }

      /*
      if (sim_meas_cycle>50000 && sim_meas_cycle<51000) {
	  roq_dump();
      }
      */
#endif

      /* service function unit release events */
      release_fu();

      /* call after commit stage but before writeback stage. 
       * writeback stage mark brcompleted in a cycle,status 
       * is changed from brcompleted to completed in the next
       * cycle,committed in next next cycle
       */
      check_brq();

      /* call execute stage*/
      writeback_stage();


      /* if not perfect cache */
      if (icache || dcache) {
	/* memory simulation */
	check_memq();

	/* check missq/writebackq */
	cache2mem();

	/* load/store queue */
	lsq_stage();
      }

      /* call issue stage*/
      issue_stage();

      /*call map stage*/
      map_stage();

      /* call decode stage*/
      decode_stage();

      /* if draining we must ensure last not speculated instruction is not
       * a mis-predicated branch. Otherwise it would be stalled at brcompleted
       * state and cannot manage to commit
       *
       * we let decode to make perfect predictions under DRAINING state so cannot
       * _stop won't be always true.
       */
      cannot_stop = (simulator_state == DRAINING) && last_non_spec_rs && 
	            last_non_spec_rs->mis_predict;

      if (simulator_state != DRAINING || cannot_stop) {
	fetch_stage();
      } else {
	if ((roq_num == 0) && (lsq_num == 0) && (fetch_num==0) && (decode_num==0)) {
	    done_draining = TRUE;
	}
      }

      
      /* Added by Wattch to update per-cycle power statistics */
      update_power_stats();
     
      
      sim_cycle++;
      if (simulator_state == MEASURING) {
	sim_meas_cycle++;
      }

      /* finish early? */
      if (opt_max_insts && sim_pop_insn >= opt_max_insts)
	return;
    }

    /* IOTA To switch from sampling to fast-forwarding, we have to advance the PC */
    regs.regs_PC = last_non_spec_rs->regs_NPC;
    if (MD_OP_FLAGS(last_non_spec_rs->op) & F_CTRL) {
      entering_spec = FALSE; 
      need_to_correct_pc = FALSE; 
      last_is_branch = FALSE;
      is_jump = last_non_spec_rs->br_taken;
      target_PC = last_non_spec_rs->mis_predict ? last_non_spec_rs->recover_PC 
	: last_non_spec_rs->pred_PC;
    }
    regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);
  }
  exit_now(0);
}

#endif


void cpu_init(void)
{
  int i;
  struct godson2_cpu *st;

  for (i=0;i<total_cpus;i++) {

    st = &cpus[i];

    st->cpuid = i;	

    st->local_active = 1;

    st->period = 1;//(st->cpuid==0)?1.0:8.0/6.0 ;
    st->cycles = 0;
    st->old_cycles = 0;
    st->old_sim_cycle = 0;
	st->start_dvfs = 0;
    st->old_sim_commit_insn = 0;
    st->UpperEndstopCounter = 0;
    st->LowerEndstopCounter = 0;
    st->PrevIPC = 0;
    st->QueueUtilization = 0;
    st->PrevQueueUtilization = 0;

#ifdef ASYNC_DVFS
    st->next_period = 1.0;//(st->cpuid==0)?1.0:8.0/6.0 ;
    
    st->voltage_changing = 0;
    st->freq_changing = 0;

    st->freq = 1000;
    st->next_freq = 1000;
#endif

#ifdef SYNC_DVFS
	st->duty = FREQ_DIVIDE;//(st->cpuid==0)?8:6;
	st->next_duty = st->duty;//(st->cpuid==0)?8:6;
	
    st->voltage = 1;
	st->voltage_downgrading = 0;
	st->voltage_upgrading = 0;
#endif
    resource_init(st);

    commit_stage_init(st);

    writeback_stage_init(st);

    cache2mem_init(st);

    lsq_init(st);

    issue_stage_init(st);

    map_stage_init(st);

    decode_stage_init(st);

    fetch_stage_init(st);

#ifdef POWER_STAT
    initial_power_stats(st);
#endif

    st->done_draining = FALSE;
	
#if 0
    /* Calculate overall sampling period if sampling is enabled */
    if (sampling_enabled) {
      st->sim_sample_period = sampling_k * sampling_munit;
    }

    /* Capture an initial backup of the empty simulation statistics */
    simoo_backup_stats(st);

    st->simulator_state = NOT_STARTED;
    /* change simulator state when sim_cycle==switchpoint */
    st->switchpoint = 0;

    st->last_il1_tag = 0;
    st->il1_tagset_mask = st->icache ? st->icache->tagset_mask : 0;
    st->last_itlb_tag = 0;
    st->itlb_tagset_mask = st->itlb ? st->itlb->tagset_mask : 0;

    st->last_il1_tag = 0;
    st->dl1_tagset_mask = st->dcache ? st->dcache->tagset_mask : 0;
    st->last_itlb_tag = 0;
    st->dtlb_tagset_mask = st->dtlb ? st->dtlb->tagset_mask : 0;
#endif
  }
  
}

int pause1 = 0;

void cpu_execute(tick_t now, struct godson2_cpu *st)
{
  int n, i;
  int done_draining = TRUE;

#ifdef SYNC_DVFS
  st->local_active = 1;
  if((sim_cycle % FREQ_DIVIDE) > (st->duty-1)) {
	st->local_active = 0;
  }
#endif

	  if (!perfect_cache) {
        /* memory simulation */
        //check_memq(st);

	    /* network on chip access */
	    //noc_stage(st);
        
		/* check each cpu's missq/writebackq */
      	//cache2mem(st);

 	    /* load/store queue */
    	//lsq_stage(st);
	  }

	  if (st->active&&st->local_active) {
		
        if (((st->roq_head+st->roq_num) % roq_ifq_size) != st->roq_tail)
          panic ("roq_head/roq_tail wedged");

        if (((st->brq_head+st->brq_num) % brq_ifq_size) != st->brq_tail)
          panic ("brq_head/brq_tail wedged");

        //if (sim_cycle>100000) 
        //  roq_dump();

        /*run through the pipeline stages*/

#ifdef POWER_STAT
        /* added for our power model to clear hardware access counters */
        clear_access_stats(st);
#endif
      
#if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
      /* check if pipetracing is still active */
      ptrace_check_active(st->regs.regs_PC, st->sim_pop_insn, sim_cycle);

      /* indicate new cycle in pipetrace */
      ptrace_newcycle(sim_cycle);
#endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      /* call the commit stage */
        n = commit_stage(st);

		extern struct inst_descript *last_commit_rs;
        /*if (n==0) {
    	  st->no_commit++;
    	  if (st->no_commit>3000000) {
			printf("no commit cpu is cpu%d , last commit pc is 0x%x , IR is %x\n",st->cpuid,last_commit_rs->regs_PC,last_commit_rs->IR);
			pause1 = 1;
	        fatal("more than 3000000 cycle no commit,probably dead locked!\n");
    	  }
        }else{
	      st->no_commit=0;
        }*/

        /* service function unit release events */
        release_fu(st);

        /* call after commit stage but before writeback stage. 
         * writeback stage mark brcompleted in a cycle,status 
         * is changed from brcompleted to completed in the next
         * cycle,committed in next next cycle
         */
        check_brq(st);

        /* call execute stage*/
        writeback_stage(st);

	  }
	  
        /* if not perfect cache */
        if (!perfect_cache) {
          /* memory simulation */
          check_memq(st);

	      /* on chip network access */
#ifdef ROUTER_SPLIT
#else
	      noc_stage(st);
#endif
	
#ifdef SYNC_DVFS
  if((sim_cycle % FREQ_DIVIDE) > (st->duty-1)) 
              return;   
#endif
#ifdef ASYNC_DVFS
          if(st->local_active) 
#endif
          { 
          /* check each cpu's missq/writebackq */
      	  cache2mem(st);

 	      /* load/store queue */
    	  lsq_stage(st);
          }
        }

	  if(st->active&&st->local_active){
        /* call issue stage*/
        issue_stage(st);

        /*call map stage*/
        map_stage(st);

        /* call decode stage*/
        decode_stage(st);

        /* call instruction fetch unit if it is not blocked */
        fetch_stage(st);

        st->sim_meas_cycle++;

        /* Added to update per-cycle power statistics */
#ifdef POWER_STAT
        update_power_stats(st);
#endif
	    if ((sim_cycle > 3000) && (st->roq_num == 0) && (st->lsq_num == 0) && (st->fetch_num==0) && (st->decode_num==0)) {
	      st->done_draining = TRUE;
	    }
     
	    done_draining &= st->done_draining;
	  
  	    /* finish early? */
        if (opt_max_insts && st->sim_pop_insn >= opt_max_insts)
    	  return;
	  }

//	if (done_draining)  return;

//DVFS algorithm here
#ifdef SYNC_DVFS
	  if(st->sim_commit_insn - st->old_sim_commit_insn >= SAMPLING_PERIOD) 
	  if(!st->start_dvfs){
		st->start_dvfs = 1;
	  double IPC = SAMPLING_PERIOD/(double)(sim_cycle - st->old_sim_cycle);
	  st->QueueUtilization = st->QueueUtilization/(st->cycles - st->old_cycles);

/*	  fprintf(fp[st->cpuid],"sim_cycle:%lld \t st->old_sim_cycle: %lld\n",sim_cycle,st->old_sim_cycle);
	  fprintf(fp[st->cpuid],"st->sim_commit_insn:%lld \t st->old_sim_commit_insn: %lld\n",st->sim_commit_insn,st->old_sim_commit_insn);  
	  fprintf(fp[st->cpuid],"\n--------start dvfs------------\n");
*/	  st->PrevIPC = IPC;
	  st->PrevQueueUtilization = st->QueueUtilization;
      st->old_sim_commit_insn = st->sim_commit_insn;
	  st->old_sim_cycle = sim_cycle;
	  st->old_cycles = st->cycles;
      st->QueueUtilization = 0;
	  }
      else {
//	  double PeriodScaleFactor = 1.0;
	  double IPC = SAMPLING_PERIOD/(double)(sim_cycle - st->old_sim_cycle);
	  st->QueueUtilization = st->QueueUtilization/(st->cycles - st->old_cycles);

/*	  fprintf(fp[st->cpuid],"sim_cycle:%lld \t st->old_sim_cycle: %lld \t diff:%lld\n",sim_cycle,st->old_sim_cycle,sim_cycle-st->old_sim_cycle);
	  fprintf(fp[st->cpuid],"st->sim_commit_insn:%lld \t st->old_sim_commit_insn: %lld\n",st->sim_commit_insn,st->old_sim_commit_insn);  
	  fprintf(fp[st->cpuid],"old duty: %d\n",st->duty);
	  fprintf(fp[st->cpuid],"PrevQueueUtilization:%lf\tQueueUtilization:%lf\n",st->PrevQueueUtilization,st->QueueUtilization);
	  fprintf(fp[st->cpuid],"PrevIPC:%lf\tIPC:%lf\n",st->PrevIPC,IPC);
*/
	  if(st->UpperEndstopCounter == 10)
		st->next_duty -= FREQ_STEP;
	  else if(st->LowerEndstopCounter == 10)
		st->next_duty += FREQ_STEP;
      else if((st->QueueUtilization - st->PrevQueueUtilization) > (st->PrevQueueUtilization * DeviationThreshold))
		st->next_duty += FREQ_STEP;
	  else if((st->PrevQueueUtilization - st->QueueUtilization) > (st->PrevQueueUtilization * DeviationThreshold) && ((st->PrevIPC-IPC)/st->PrevIPC <= PerfDegThreshold))
		st->next_duty -= FREQ_STEP;
//	  else if(((IPC-st->PrevIPC)/st->PrevIPC >= PerfDegThreshold) /*&& (st->QueueUtilization >= st->PrevQueueUtilization)*/)
//		st->next_duty -= FREQ_STEP;

	  if(st->next_duty<=MINIMUM_DUTY)
		st->next_duty = MINIMUM_DUTY;
	  if(st->next_duty>=MAXIMUM_DUTY)
		st->next_duty = MAXIMUM_DUTY;
  
	  st->PrevIPC = IPC;
	  st->PrevQueueUtilization = st->QueueUtilization;
	  if((st->next_duty <= MINIMUM_DUTY)&&(st->LowerEndstopCounter != 10))
		st->LowerEndstopCounter++;
	  else st->LowerEndstopCounter = 0;
	  if((st->next_duty >= MAXIMUM_DUTY)&&(st->UpperEndstopCounter != 10))
		st->UpperEndstopCounter++;
	  else st->UpperEndstopCounter = 0;

//	  fprintf(fp[st->cpuid],"new next_duty: %d\n\n",st->next_duty);
      st->old_sim_commit_insn = st->sim_commit_insn;
	  st->old_sim_cycle = sim_cycle;
	  st->old_cycles = st->cycles;
      st->QueueUtilization = 0;
   
	  }
    if(st->active) st->cycles++;
#else
#ifdef ASYNC_DVFS
  if(st->sim_commit_insn-st->old_sim_commit_insn >= SAMPLING_PERIOD) {
    if(!st->start_dvfs){ //for the 1st DVFS during runtime
        st->start_dvfs = 1;

        double IPC = SAMPLING_PERIOD/(double)(sim_cycle - st->old_sim_cycle);

/*
        fprintf(fp[st->cpuid],"st->cycles:%lld \t st->old_cycles: %lld diff:%lld\n",st->cycles,st->old_cycles,st->cycles-st->old_cycles);  
        fprintf(fp[st->cpuid],"sim_cycle:%lf \t st->old_sim_cycle: %lf\t diff:%lf\n",sim_cycle,st->old_sim_cycle,sim_cycle - st->old_sim_cycle);  
        fprintf(fp[st->cpuid],"st->sim_commit_insn:%lld \t st->old_sim_commit_insn: %lld\n",st->sim_commit_insn,st->old_sim_commit_insn);  
        fprintf(fp[st->cpuid],"PrevQueueUtilization:%lf\tQueueUtilization:%lf\n",st->PrevQueueUtilization,st->QueueUtilization);
        fprintf(fp[st->cpuid],"PrevIPC:%lf\tIPC:%lf\n",st->PrevIPC,IPC);
        fprintf(fp[st->cpuid],"\n--------start dvfs------------\n");
*/	  
        st->PrevIPC = IPC;
        st->QueueUtilization = st->QueueUtilization/(st->cycles - st->old_cycles);

        st->PrevQueueUtilization = st->QueueUtilization;
        st->old_sim_commit_insn = st->sim_commit_insn;
        st->QueueUtilization = 0;
        st->old_sim_cycle = sim_cycle;
        st->old_cycles = st->cycles;
    }
    else {
//      double PeriodScaleFactor = 1.0;
        double IPC = SAMPLING_PERIOD/(double)(sim_cycle - st->old_sim_cycle);
	  
        st->QueueUtilization = st->QueueUtilization/(st->cycles - st->old_cycles);

        if(st->UpperEndstopCounter == 10)
            st->next_freq = st->freq - ReactionChange;
	else if(st->LowerEndstopCounter == 10)
            st->next_freq = st->freq + ReactionChange;
        else if((st->QueueUtilization - st->PrevQueueUtilization) > (st->PrevQueueUtilization * DeviationThreshold))
            st->next_freq = st->freq + ReactionChange;
        else if((st->PrevQueueUtilization - st->QueueUtilization) > (st->PrevQueueUtilization * DeviationThreshold) && (st->PrevIPC-IPC)/st->PrevIPC <= PerfDegThreshold)
            st->next_freq = st->freq - ReactionChange;
//      else if(((IPC-st->PrevIPC)/st->PrevIPC) <= PerfDegThreshold && (st->QueueUtilization - st->PrevQueueUtilization < 0))
//          st->next_freq = st->freq - Decay;
	else if(((st->PrevIPC-IPC)/st->PrevIPC) <= PerfDegThreshold)
            st->next_freq = st->freq - Decay;

/*
        fprintf(fp[st->cpuid],"cpuid:%d\nold period: %lf\t old freq:%d\n",st->cpuid,st->period,st->freq);
        fprintf(fp[st->cpuid],"st->cycles:%lld \t st->old_cycles: %lld diff:%lld\n",st->cycles,st->old_cycles,st->cycles-st->old_cycles);  
        fprintf(fp[st->cpuid],"sim_cycle:%lf \t st->old_sim_cycle: %lf\t diff:%lf\n",sim_cycle,st->old_sim_cycle,sim_cycle - st->old_sim_cycle);  
        fprintf(fp[st->cpuid],"st->sim_commit_insn:%lld \t st->old_sim_commit_insn: %lld\n",st->sim_commit_insn,st->old_sim_commit_insn);  
        fprintf(fp[st->cpuid],"PrevQueueUtilization:%lf\tQueueUtilization:%lf\n",st->PrevQueueUtilization,st->QueueUtilization);
        fprintf(fp[st->cpuid],"PrevIPC:%lf\tIPC:%lf\n",st->PrevIPC,IPC);
*/
        if(st->next_freq <= MINIMUM_FREQ)
            st->next_freq = MINIMUM_FREQ;
        if(st->next_freq >= MAXIMUM_FREQ)
            st->next_freq = MAXIMUM_FREQ;
	st->next_period = 1000.0/(double)st->next_freq;
	  
//      fprintf(fp[st->cpuid],"new period: %lf\tnew freq:%d\n\n",st->next_period,st->next_freq);
	  
        if((st->next_freq <= MINIMUM_FREQ)&&(st->LowerEndstopCounter != 10))
            st->LowerEndstopCounter++;
        else st->LowerEndstopCounter = 0;
        
        if((st->next_freq >= MAXIMUM_FREQ)&&(st->UpperEndstopCounter != 10))
           st->UpperEndstopCounter++;
         else st->UpperEndstopCounter = 0;

        st->PrevIPC = IPC;
        st->PrevQueueUtilization = st->QueueUtilization;

        st->old_sim_commit_insn = st->sim_commit_insn;
        st->QueueUtilization = 0;
        st->old_sim_cycle = sim_cycle;
        st->voltage_changing = 1;	
        st->old_cycles = st->cycles;
        st->old_sim_cycle = sim_cycle;
    }
  }
	  
  if((sim_cycle - st->old_sim_cycle >= SAMPLING_PERIOD/10) && st->voltage_changing)
  {
      st->local_active = 0;
      st->voltage_changing = 0;
      st->freq_changing = 1;
  }
  if((sim_cycle - st->old_sim_cycle >= 2 * SAMPLING_PERIOD/10) && st->freq_changing)
  {
      printf("cpuid:%d\nold period: %lf\n",st->cpuid,st->period);
      st->period = st->next_period;
      st->freq = st->next_freq;
      st->local_active = 1;
      st->freq_changing = 0;
      printf("new period: %lf\n",st->next_period);
  }

 if(st->active&&st->local_active) st->cycles++; 
#endif //end of ASYNC_DVFS

  eventq_queue_callback(now + st->period,(void*)cpu_execute,(int)st);
#endif 
//end of DVFS algorithm
    
}

#ifdef SYNC_DVFS
void
sim_main(void) {
  int n, i;
  int done_draining = TRUE;
  struct godson2_cpu *st;

  void exit_now(int);
/*
	fp[0]=fopen("fname_0.txt" ,"w");
	fp[1]=fopen("fname_1.txt" ,"w");
	fp[2]=fopen("fname_2.txt" ,"w");
	fp[3]=fopen("fname_3.txt" ,"w");
	fp[4]=fopen("fname_4.txt" ,"w");
	fp[5]=fopen("fname_5.txt" ,"w");
	fp[6]=fopen("fname_6.txt" ,"w");
	fp[7]=fopen("fname_7.txt" ,"w");
	fp[8]=fopen("fname_8.txt" ,"w");
	fp[9]=fopen("fname_9.txt" ,"w");
	fp[10]=fopen("fname_10.txt" ,"w");
	fp[11]=fopen("fname_11.txt" ,"w");
	fp[12]=fopen("fname_12.txt" ,"w");
	fp[13]=fopen("fname_13.txt" ,"w");
	fp[14]=fopen("fname_14.txt" ,"w");
	fp[15]=fopen("fname_15.txt" ,"w");
*/
  
  /* initialize all cpu states */
  cpu_init();

//  run_fast_sequential();
  
//  printf("sim_cycle after sequential:%d\n",sim_cycle);
  
  cpus[0].fetch_reg_PC = cpus[0].regs.regs_PC;
  
  sim_cycle = 0;
  
/*
  for (i=0;i<total_cpus;i++) {
      st = &cpus[i];
      eventq_queue_callback(0,(void*)cpu_execute,(int)st);
    }
*/
  while (1) {
    /* Service memory hierarchy events */
 
    eventq_service_events(sim_cycle);
	
    for (i=0;i<total_cpus;i++) {
      st = &cpus[i];
      cpu_execute(sim_cycle,st);
   }
    
    sim_cycle = sim_cycle + 1;

//change duty
    for (i=0;i<total_cpus;i++) {
      st = &cpus[i];
 	  if(sim_cycle%FREQ_DIVIDE==0){
	
		if(st->duty > st->next_duty){
        printf("cpuid:%d change duty to %d\n", st->cpuid, st->next_duty);
    		st->duty = st->next_duty;
            st->voltage_downgrading = 1;
		}
		else if(st->duty < st->next_duty){
            st->voltage_upgrading = 1;
			st->voltage = (st->duty == 4)?0.5:(st->duty == 2)?0.25:(st->duty == 1)?0.125:1;
		}

	    if((sim_cycle - st->old_sim_cycle >= SAMPLING_PERIOD/10) && st->voltage_downgrading)
	  {
		st->voltage_downgrading = 0;
		st->voltage = (st->duty == 4)?0.5:(st->duty == 2)?0.25:(st->duty == 1)?0.125:1;
	  }
	    else if((sim_cycle - st->old_sim_cycle >= SAMPLING_PERIOD/10) && st->voltage_upgrading)
	  {
		st->voltage_upgrading = 0;
    	st->duty = st->next_duty;
        printf("cpuid:%d change duty to %d\n", st->cpuid, st->next_duty);
	  }
     }
    }
}

}

#else //for asynchronous and original CMP

/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void) {
  int n, i;
  int done_draining = TRUE;
  struct godson2_cpu *st;

  /* initialize all cpu states */
  cpu_init();

//  run_fast_sequential();

//  printf("sim_cycle after sequential:%lf\n",sim_cycle);
  
  cpus[0].fetch_reg_PC = cpus[0].regs.regs_PC;
 
  for (i=0;i<total_cpus;i++) {
      st = &cpus[i];
#ifdef ROUTER_SPLIT
      eventq_queue_callback(0,(void*)noc_stage,(int)st);
#endif
      eventq_queue_callback(0,(void*)cpu_execute,(int)st);
//      eventq_queue_callback(0,(void*)check_memq,(int)st);
    }
  
  eventq_service_events(0);
}
#endif
