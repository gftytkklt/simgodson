/*
 * dram.h - header file for dram.c
 * dram.c - dram access routines. simulates sdram/ddr
 *
 * this file is being contributed to the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * Copyright (c) 2001, 2000, 1999 by Vinodh Cuppu and Bruce Jacob
 * Vinodh Cuppu, Bruce Jacob
 * Systems and Computer Architecture Lab
 * Dept of Electrical & Computer Engineering
 * University of Maryland, College Park
 * All Rights Reserved
 *
 * Distributed as part of the sim-alpha release
 * Copyright (C) 1999 by Raj Desikan
 * This software is distributed with *ABSOLUTELY NO SUPPORT* and
 * *NO WARRANTY*.  Permission is given to modify this code
 * as long as this notice is not removed.
 *
 * Send feedback to Vinodh Cuppu ramvinod@eng.umd.edu / ramvinod@computer.org
 *               or Bruce Jacob blj@eng.umd.edu
 */

/* nothing yet */

/*
 * these variables are defined in dram.c
 * for comments, look it up in dram.c
 */

extern int clock_multiplier;
extern int page_policy;
extern int ras_delay;
extern int cas_delay;
extern int pre_delay;
extern int data_rate;
extern int chipset_delay_req;
extern int chipset_delay_return;
extern int bus_width;
extern int rc_delay;

#define OP  0
#define CPA 1

extern void dram_init();
extern int
dram_access_latency( int cmd,        /* read or write */
		      md_addr_t addr, /* address to read from */
		      int bsiz,       /* size to read/write */
		      tick_t now );    /* what's the cycle number now */
