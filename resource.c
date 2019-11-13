/*
 * resource.c - resource manager routines
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "host.h"
#include "misc.h"
#include "resource.h"
#include "mips.h"
#include "godson2_cpu.h"

/*
 * functional unit resource configuration
 */

/* resource pool indices, NOTE: update these if you change FU_CONFIG */
#define FU_IALU_INDEX			0
#define FU_IMULT_INDEX			1
#define FU_MEMPORT_INDEX		2
#define FU_FPALU_INDEX			3
#define FU_FPMULT_INDEX			4

/* resource pool definition, NOTE: update FU_*_INDEX defs if you change this */
struct res_desc fu_config[] = {
  {
    "ALU1",
    1,
    0,
    0,
    {
      { IntALU, 1, 1 },
      { IntBR, 1, 1 }
    }
  },
  {
    "ALU2/MULT/DIV",
    1,
    0,
    0,
    {
      { IntALU, 1, 1 },
      { IntMULT, 4, 1 },
      { IntDIV, 13, 12 }
    }
  },
  {
    "memory-port",
    1,
    0,
    0,
    {
      { RdPort, 4, 1 },
      { WrPort, 4, 1 }
    }
  },
  {
    "FP-adder",
    1,
    0,
    0,
    {
      { FpBR, 3, 1 },
      { FloatADD, 3, 1 },
      { FloatCMP, 3, 1 },
      { FloatCVT, 3, 1 }
    }
  },
  {
    "FP-MULT/DIV",
    1,
    0,
    0,
    {
      { FloatMULT, 4, 1 },
      { FloatDIV, 4, 1 },
      { FloatSQRT, 4, 1 }
    }
  },
};


/* create a resource pool */
struct res_pool *
res_create_pool(char *name, struct res_desc *pool, int ndesc)
{
  int i, j, k, index, ninsts;
  struct res_desc *inst_pool;
  struct res_pool *res;

  /* count total instances */
  for (ninsts=0,i=0; i<ndesc; i++)
    {
      if (pool[i].quantity > MAX_INSTS_PER_CLASS)
        fatal("too many functional units, increase MAX_INSTS_PER_CLASS");
      ninsts += pool[i].quantity;
    }

  /* allocate the instance table */
  inst_pool = (struct res_desc *)calloc(ninsts, sizeof(struct res_desc));
  if (!inst_pool)
    fatal("out of virtual memory");

  /* fill in the instance table */
  for (index=0,i=0; i<ndesc; i++)
    {
      for (j=0; j<pool[i].quantity; j++)
	{
	  inst_pool[index] = pool[i];
	  inst_pool[index].quantity = 1;
	  inst_pool[index].busy = FALSE;
	  inst_pool[index].alloc_stamp = 0;
	  for (k=0; k<MAX_RES_CLASSES && inst_pool[index].x[k].class; k++) {
	    inst_pool[index].x[k].master = &inst_pool[index];
	    inst_pool[index].x[k].fu_index = index;
	    inst_pool[index].x[k].busy = 0;
	  }
	  index++;
	}
    }
  assert(index == ninsts);

  /* allocate the resouce pool descriptor */
  res = (struct res_pool *)calloc(1, sizeof(struct res_pool));
  if (!res)
    fatal("out of virtual memory");
  res->name = name;
  res->num_resources = ninsts;
  res->resources = inst_pool;

  /* fill in the resource table map - slow to build, but fast to access */
  for (i=0; i<ninsts; i++)
    {
      struct res_template *plate;
      for (j=0; j<MAX_RES_CLASSES; j++)
	{
	  plate = &res->resources[i].x[j];
	  if (plate->class)
	    {
	      assert(plate->class < MAX_RES_CLASSES);
	      res->table[plate->class][res->nents[plate->class]++] = plate;
	    }
	  else
	    /* all done with this instance */
	    break;
	}
    }

  return res;
}

/* get a free resource from resource pool POOL that can execute a
   operation of class CLASS, returns a pointer to the resource template,
   returns NULL, if there are currently no free resources available,
   follow the MASTER link to the master resource descriptor;
   NOTE: caller is responsible for reseting the busy flag in the beginning
   of the cycle when the resource can once again accept a new operation */
struct res_template *
res_get(struct res_pool *pool, int class)
{
  int i;

  /* must be a valid class */
  assert(class < MAX_RES_CLASSES);

  /* must be at least one resource in this class */
  assert(pool->table[class][0]);

  for (i=0; i<MAX_INSTS_PER_CLASS; i++)
    {
      if (pool->table[class][i])
	{
	  if (!pool->table[class][i]->master->busy)
	    return pool->table[class][i];
	}
      else
	break;
    }
  /* none found */
  return NULL;
}

/* dump the resource pool POOL to stream STREAM */
void
res_dump(struct res_pool *pool, FILE *stream)
{
  int i, j;

  if (!stream)
    stream = stderr;

  fprintf(stream, "Resource pool: %s:\n", pool->name);
  fprintf(stream, "\tcontains %d resource instances\n", pool->num_resources);
  for (i=0; i<MAX_RES_CLASSES; i++)
    {
      fprintf(stream, "\tclass: %d: %d matching instances\n",
	      i, pool->nents[i]);
      fprintf(stream, "\tmatching: ");
      for (j=0; j<MAX_INSTS_PER_CLASS; j++)
	{
	  if (!pool->table[i][j])
	    break;
	  fprintf(stream, "\t%s (busy for %d cycles) ",
		  pool->table[i][j]->master->name,
		  pool->table[i][j]->master->busy);
	}
      assert(j == pool->nents[i]);
      fprintf(stream, "\n");
    }
}


void resource_init(struct godson2_cpu *st)
{
  st->fu_pool = res_create_pool("fu-pool", fu_config, N_ELT(fu_config));
}

/* service all functional unit release events, this function is called
   once per cycle, and it used to step the BUSY timers attached to each
   functional unit in the function unit resource pool, as long as a functional
   unit's BUSY count is > 0, it cannot be issued an operation */
void
release_fu(struct godson2_cpu *st)
{
  int i;

  /* walk all resource units, decrement busy counts by one */
  for (i=0; i<st->fu_pool->num_resources; i++)
    {
      /* resource is released when BUSY hits zero */
      if (st->fu_pool->resources[i].busy > 0)
    	st->fu_pool->resources[i].busy--;
    }
}

struct res_template * my_res_get(struct godson2_cpu *st, int class)
{
  return res_get(st->fu_pool,class);
}

int is_intq_op(int op)
{
  int fu;

  fu = MD_OP_FUCLASS(op);

  return (fu==IntALU ||		
	      fu==IntBR  ||
          fu==IntMULT||
          fu==IntDIV ||	  
    	  fu==RdPort ||
    	  fu==WrPort);
}

/* allocate a resource from resource pool POOL that can execute a
   operation of class CLASS, returns a pointer to the resource template,
   don't care whether the resource is busy. Used by map stage to allocate
   function units.
 */
struct res_template * res_alloc (struct godson2_cpu *st, int class)
{
  int i;
  struct res_pool *pool = st->fu_pool;
  struct res_template * fu;
  static unsigned int alloc_stamp = 0;

  /* must be a valid class */
  assert(class < MAX_RES_CLASSES);

  /* must be at least one resource in this class */
  assert(pool->table[class][0]);

  /* only one usable */
  if (pool->nents[class]==1) {
	fu = pool->table[class][0];
  } else {
	/* choose the function unit that has not been allocated for the longest time */
	fu = pool->table[class][0];
	for (i=1;i<pool->nents[class];i++) {
	  if (pool->table[class][i]->master->alloc_stamp < fu->master->alloc_stamp)
	    fu = pool->table[class][i];
	}
  }
  fu->master->alloc_stamp = alloc_stamp++;
  return fu;
}

int res_get_fu_number(struct godson2_cpu *st)
{
  if (st->fu_pool==NULL) {
	printf("call resource_init before this!\n");
  }
  return st->fu_pool->num_resources;
}

