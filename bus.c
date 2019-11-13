/*
 * bus.c - bus module routines
 *
 * This file is a part of the SimpleScalar tool suite, and was 
 * written by Doug Burger, as a part of the Galileo research project.  
 * Alain Kagi has also contributed to this code.
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1996, 1997 by Doug Burger
 * Copyright (C) 1999 by Raj Desikan
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

#include <assert.h>
#include <stdlib.h>

#include "mips.h"
#include "misc.h"
#include "cache.h"
#include "bus.h"

/* Bus definition counter */
int bus_nelt = 0;

/* Array of cache strings */
char *bus_configs[MAX_NUM_BUSES];

/* Array of bus pointers */
struct bus *buses[MAX_NUM_BUSES];

int num_buses;

int bus_queuing_delay;

/*
 * simulates contention, bus is the resource, returns a number of cycles
 * corresponding to the delay caused by contention.  the resource can be idle
 * in which case there is no contention (return 0).  the resource can be busy
 * in which case the function returns the amount of delay.
 */
tick_t
bus_access(tick_t arr_time, 
	   int request_size, 
	   struct bus *bus, 
	   int request_type)
{
  tick_t time_diff, service_time, response_time;
  
  bus->requests++;

  /* asserting causality */
  //assert(arr_time >= bus->last_req);
  if (arr_time < bus->last_req)
    panic("arr_time > last req time");
  bus->last_req = arr_time;

  /* TODO: This is a hack for Rambus */
  if (bus->resource_code == SIMPLE_RAMBUS)
    {
      if (request_type == Read_Request)
	{
	  response_time = service_time = bus->clock_differential * 6;
	}
      else if (request_type == Read_Response)
	{
	  /* If the data transmittal is less than bus width, fix the code to
	     round up */
	  assert(request_size > bus->width);
	  response_time = bus->clock_differential;
	  service_time = bus->clock_differential * (request_size/bus->width);
	}
      else /* (request_type == Write_Request) */
	{
	  /* If the data transmittal is less than bus width, fix the code to
	     round up */
	  assert(request_size > bus->width);
	  response_time = service_time = bus->clock_differential * 
	    (6 + 1 + request_size/bus->width);
	}
    }
  else
    {
      response_time = bus->clock_differential * (bus->arbitration + 1);
      service_time = bus->clock_differential * (bus->arbitration + ceil((float)request_size/bus->width));
    }
  
  time_diff = bus->time - arr_time;

#ifdef TRACE_BUS
if (time_diff > 0)
  fprintf(stderr, "\t\t\t\t%s BUS %s, %u bytes, %u bus cycles of data, time %u\n\t\t\t\twait %u, requires %u proc. cycles, bus free at %u\n", 
	  bus->name, (request_type == Read_Request) ? "read request" :
	  ((request_type == Write_Request) ? "write back" : "read transmission"), 
	  (unsigned int) request_size, (unsigned int) request_size/bus->width, 
	  (unsigned int) arr_time, (unsigned int) time_diff, (unsigned int) service_time, 
	  (unsigned int) (bus->time+service_time));
else
  fprintf(stderr, "\t\t\t\t%s BUS %s, %u bytes, %u bus cycles of data, time %u\n\t\t\t\twait %u, requires %u proc. cycles, bus free at %u\n", 
	  bus->name, (request_type == Read_Request) ? "read request" :
	  ((request_type == Write_Request) ? "write back" : "read transmission"), 
	  (unsigned int) request_size, (unsigned int) request_size/bus->width, 
	  (unsigned int) arr_time, (unsigned int) 0, (unsigned int) service_time, 
	  (unsigned int) (arr_time+service_time));
#endif

  if (bus_queuing_delay && time_diff > 0)
    {
      /* queueing delay */
      bus->qdly += time_diff;
      bus->time += service_time;
      return(time_diff + response_time);
    }
  else
    {
      /* bus was idle */
      bus->idle -= time_diff;
      bus->time = arr_time + service_time;
      return(response_time);
    }
}

/* Returns the structure under the bus, uses the resource code to 
 * determine which one (unless there is only one).  The type (cache
 * or memory) is returned in type, so that the caller will know which
 * function to eventually call in the callback */

void *
bus_follow(struct bus *bp, 
	   enum resource_type *type)
{
  int index = 0;

  if (bp->num_resources > 1)
  {
    switch(bp->resource_code) {
  case 0:
      break;
  default:	
      break;
    }
  }
  *type = bp->resource_type[index];
  return((void *)bp->resources[index]);
}

struct bus *
bus_select(int num, 
	   int code, 
	   void **resources)
{
  int index = 0;
  struct bus *bp = NULL;

  assert(num > 0);
  assert(resources);

  /* If there's only one bus under this cache, return that */
  /* Right now having more than one bus isn't implemented */
  if (num > 1)
  {
    switch (code) {
  case 0:
      index = 0;
    break;
  default:
      index = 0;
    break;
    }
  }
  bp = (struct bus *) resources[index];

  assert(bp);
  return(bp);
}

struct bus *bus_create(char *name, 
		       int width, 
		       int clock_diff, 
		       int arbitration, 
		       int inf_bandwidth, 
		       int resources, 
		       int resource_code, 
		       char *res_names[])
{
  struct bus *temp;
  int i;

  if (width < 1 || (width & (width-1)) != 0)
    fatal("memory bus width must be positive non-zero and a power of two");

  if (clock_diff < 1)
    fatal("memory bus clock differential must be positive non-zero");

  if (arbitration < 0)
    fatal("memory bus arbitration penalty may not be negative");

  temp = (struct bus *) malloc(sizeof(struct bus));
  temp->name = (char *) mystrdup(name);
  temp->time = 0;
  temp->idle = 0;
  temp->qdly = 0;
  temp->requests = 0;
  temp->last_req = 0;
  temp->inf_bandwidth = (inf_bandwidth != 0);
  temp->clock_differential = clock_diff;
  temp->width = width;
  temp->arbitration = arbitration;
  temp->num_resources = resources;
  temp->resource_code = resource_code;
  temp->select_function = 0;
  for (i=0; i<resources; i++)
    {
      temp->resource_names[i] = strdup(res_names[i]);
    }
  return(temp);
}

/* register bus stats */

void
bus_reg_stats(struct bus *bp,		/* bus instance */
	      struct stat_sdb_t *sdb)	/* stats database */
{
  char buf[512], buf1[512], *name;

  /* get a name for this buf */
  if (!bp->name || !bp->name[0])
    name = "<unknown>";
  else
    name = bp->name;

  sprintf(buf, "%s.idle", name);
  sprintf(buf1, "%s.idle_cycles / sim_cycle", name);
  stat_reg_formula(sdb, buf, "fraction of time bus is idle", buf1, NULL);

  sprintf(buf, "%s.queued", name);
  sprintf(buf1, "%s.queued_cycles / %s.requests", name, name);
  stat_reg_formula(sdb, buf, "average queueing delay seen by bus request", buf1, NULL);

  sprintf(buf, "%s.requests", name);
  stat_reg_counter(sdb, buf, "number of transmissions on bus", &bp->requests, 0, NULL);

  sprintf(buf, "%s.idle_cycles", name);
  stat_reg_counter(sdb, buf, "number of cycles bus was idle", &bp->idle, 0, NULL);

  sprintf(buf, "%s.queued_cycles", name);
  stat_reg_counter(sdb, buf, "total number of queued cycles for all requests", &bp->qdly, 0, NULL);
}

