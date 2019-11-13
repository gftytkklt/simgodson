/*
 * bus.h - bus module interfaces
 *
 * This file is a part of the SimpleScalar tool suite, and was 
 * written by Doug Burger, as a part of the Galileo research project.  
 * Alain Kagi has also contributed to this code.
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1996, 1997 by Doug Burger
 * Distributed as part of the sim-alpha release
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

#ifndef BUS_H
#define BUS_H

#define MAX_NUM_BUSES  10

#define Read_Request 	0
#define Write_Request	1
#define Read_Response	2

struct bus {
  char *name;
  tick_t time;	/* time when bus to next level is free */
  tick_t idle;	/* amount of time the bus was idle */
  tick_t qdly;	/* queueing delay experienced by requests */
  tick_t last_req;	/* time of last request */
  tick_t clock_differential;	/* proc freq/bus freq */
  counter_t requests;	/* Number of requests on bus */
  int width;		/* Width of the bus, in bytes */
  int arbitration;	/* Number of cycles to arbitrate */
  int inf_bandwidth;	/* Whether the bus is modeled with contention or not 
			   A "1" value here means all transmissions take one
			   cycle, no matter what the transfer size */
  int num_resources;	/* Number of things below the bus */
  int resource_code;	/* Allows choosing of the direction things may go in a bus
			   with multiple resources attached */
  void *resources[MAX_NUM_RESOURCES];	/* Pointers to things below the bus */
  char *resource_names[MAX_NUM_RESOURCES]; /* Names of things below the bus */
  enum resource_type resource_type[MAX_NUM_RESOURCES];	/* What are things below the bus? */
  int select_function;	/* function to choose which resource below the bus */
};

struct bus *
bus_create(char *name, 
	   int width, 
	   int clock_diff, 
	   int arbitration, 
	   int inf_bandwidth, 
	   int resources, 
	   int resource_code, 
	   char *res_names[]);

tick_t 
bus_access(tick_t arr_time, 
	   int request_size, 
	   struct bus *bus, 
	   int request_type);

struct bus *
bus_select(int num, 
	   int code, 
	   void **resources);

void *
bus_follow(struct bus *bp, 
	   enum resource_type *type);

void bus_reg_stats(struct bus *bp, 
		   struct stat_sdb_t *sdb);

extern int bus_nelt;
extern char *bus_configs[MAX_NUM_BUSES];
extern struct bus *buses[MAX_NUM_BUSES];
extern int num_buses;
extern int bus_queuing_delay;

#endif

