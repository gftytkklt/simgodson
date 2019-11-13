/*
 * sim.h - simulator main line interfaces
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
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
 * INTERNET: dburger@cs.wisc.edu
 * US Mail:  1210 W. Dayton Street, Madison, WI 53706
 *
 */

#ifndef SIM_H
#define SIM_H

#include <stdio.h>
#include <setjmp.h>
#include <time.h>

#include "options.h"
#include "stats.h"
#include "regs.h"
#include "cache.h"
#include "godson2_cpu.h"

/* set to non-zero when simulator should dump statistics */
extern int sim_dump_stats;

/* exit when this becomes non-zero */
extern int sim_exit_now;

/* longjmp here when simulation is completed */
extern jmp_buf sim_exit_buf;

/* byte/word swapping required to execute target executable on this host */
extern int sim_swap_bytes;
extern int sim_swap_words;

#if 0
/*
 * simulator stats
 */
/* execution instruction counter */
extern counter_t sim_pop_insn;
extern counter_t sim_slip ;

/* total number of instructions measured */
extern counter_t sim_total_insn;

/* total number of memory references committed */
extern counter_t sim_num_refs ;

/* total number of memory references executed */
extern counter_t sim_total_refs ;

/* total number of loads committed */
extern counter_t sim_num_loads ;

/* total number of loads executed */
extern counter_t sim_total_loads ;

/* total number of branches committed */
extern counter_t sim_num_branches ;

/* total number of branches executed */
extern counter_t sim_total_branches;

#endif

/* fasw forward insts before detailed simulation */
extern counter_t opt_fastfwd_count;

/* maximum insts to execute */
extern counter_t opt_max_insts;

/* verbose print inst interval */
extern counter_t opt_inst_interval;

/* cycle counter */
extern tick_t sim_cycle;

/* execution start/end times */
extern time_t sim_start_time;
extern time_t sim_end_time;
extern int sim_elapsed_time;

/* options database */
extern struct opt_odb_t *sim_odb;

/* stats database */
extern struct stat_sdb_t *sim_sdb;

/* EIO interfaces */
extern char *sim_eio_fname;
extern char *sim_chkpt_fname;
extern FILE *sim_eio_fd;

/* redirected program/simulator output file names */
extern FILE *sim_progfd;

/* options: presently the same for all cpus */

extern int fetch_ifq_size;
 /* maximum insts fetch at one cycle */
extern int fetch_width;
 /* maximum branches fetch at one cycle */
extern int fetch_speed;

extern int decode_pipein_bits;
extern int decode_width;
extern int decode_speed;

extern int decode_ifq_size;
extern int roq_ifq_size;

extern int brq_ifq_size;

/* maximum instruction mapped per cycle */
extern int map_width;

/* total integer rename register number */
extern int int_rename_reg_size;
/* total floating-point rename register number */
extern int fp_rename_reg_size;

extern int issue_width;
extern int sim_has_bypass;
extern int int_issue_ifq_size;
extern int fp_issue_ifq_size;

extern int commit_width;

extern int lsq_ifq_size;
extern int missq_ifq_size;
extern int wtbkq_ifq_size;
extern int extinvnq_ifq_size;
extern int router_ifq_size;

/***************************************************/
extern int bht_nsets;
extern int bht_lognsets;
extern int btb_size;
extern int btb_pc_low;
extern int btb_pc_high;
extern int va_size;
extern int pa_size;

extern int fix_wb_width;
extern int fp_wb_width;
extern int map_pipein_bits;
extern int map_pipeout_bits;
extern int fix_phy_num;
extern int fix_phy_lognum;
extern int fp_phy_num;
extern int fp_phy_lognum;
extern int fix_state_bits;
extern int fp_state_bits;

extern int fix_issue_width;
extern int fp_issue_width;
extern int fixq_misc_bits;
extern int fpq_misc_bits;

extern int fix_regfile_pipein_bits;
extern int fp_regfile_pipein_bits;
extern int fp_fcr_reg_bits;

extern int ialu_num;
extern int falu_num;
extern int data_width;
extern int ialu_pipein_bits;
extern int ialu_pipeout_bits;
extern int falu_pipein_bits;
extern int falu_pipeout_bits;

extern int roq_state_bits;
extern int roq_bits;
extern int brq_state_bits;
extern int brq_bits;

extern int lsq_wb_width;
extern int lsq_state_bits;
extern int lsq_bits;

extern int icache_pipein_bits;
extern int itlb_pipein_bits;
extern int dcache_pipein_bits;
extern int dtlb_pipein_bits;
extern int cache_dl2_pipein_bits;
extern int cache_dl2_sramin_bits;
extern int cache_dl2_sramout_bits;
#define NUM_TYPES 4
extern int input_buffer_bits[NUM_TYPES];
#undef NUM_TYPES
extern int refill_width;
extern int missq_bits;
extern int wtbkq_bits;
extern int io_bits;

/***************************************************/
extern int perfect_cache;

 /* simple memory delay params */
extern int mem_read_first_delay;
extern int mem_read_interval_delay;
extern int mem_write_delay;

extern int memq_ifq_size;

//extern int memq_num;
//extern struct memory_queue *memq_head,*memq_free_list;

//extern struct refill_bus refill;

extern char *dcache_name;
extern char *icache_name;
extern int cache_nelt;
extern char *cache_configs[MAX_NUM_CACHES];
extern int num_caches;
extern struct cache *caches[MAX_NUM_CACHES];
extern int flush_on_syscalls;
extern int victim_buf_ent;
extern int victim_buf_lat;

#define MAX_CPUS 16 
#define MAX_ARGC 16 
#define MAX_STRING_LENGTH 128

extern int total_cpus;
extern struct godson2_cpu cpus[MAX_CPUS];

extern int mesh_width;
extern int mesh_height;

/*
 * main simulator interfaces, called in the following order
 */

/* register simulator-specific options */
void sim_reg_options(struct opt_odb_t *odb);

/* main() parses options next... */

void compute_formula();

/* check simulator-specific option values */
void sim_check_options(struct opt_odb_t *odb, int argc, char **argv);

/* register simulator-specific statistics */
void sim_reg_stats(struct stat_sdb_t *sdb);

/* initialize the simulator */
void sim_init(void);

/* load program into simulated state */
void sim_load_prog(char *fname, int argc, char **argv, char **envp);

/* check multi program arguments */
void check_multiprog_args(char *colon_num,int myargc,char **myargv,char **myenvp);

/* load multi program into simulated state */
void sim_load_multiprog();

/* load splash program into simulated state */
void sim_load_splash_prog(char *fname, int argc, char **argv, char **envp);

/* main() prints the option database values next... */

/* print simulator-specific configuration information */
void sim_aux_config(FILE *stream);

/* start simulation, program loaded, processor precise state initialized */
void sim_main(void);

/* main() prints the stats database values next... */

/* dump simulator-specific auxiliary simulator statistics */
void sim_aux_stats(FILE *stream);

/* un-initialize simulator-specific state */
void sim_uninit(void);

/* print all simulator stats */
void
sim_print_stats(FILE *fd);		/* output stream */

void doStatistics();
/* Declare simoo_restore_stats from sim-outorder.c */
void simoo_restore_stats();
void stop_measuring();
void sim_stop_tracing();

#endif /* SIM_H */
