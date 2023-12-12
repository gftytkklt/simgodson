/*
 * memory.c - flat memory space routines
 *
 * This file is part of the Alpha simulator tool suite written by
 * Raj Desikan as part of the Bullseye project.
 * It has been written by extending the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *  
 * 
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
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
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "host.h"
#include "misc.h"
#include "mips.h"
#include "sim.h"
#include "options.h"
#include "stats.h"
#include "mem.h"
#include "memory.h"
#include "loader.h"
#include "regs.h"
#include "cache.h"
#include "tlb.h"
#include "dram.h"

/* Memory bank definition counter */
int mem_nelt = 0;

/* Array of cache strings */
char *mem_configs[MAX_NUM_MEMORIES];

int num_mem_banks;

/* memory access latency (<first_chunk> <inter_chunk>) */
int mem_lat[2] = { /* lat to first chunk */18, 
		   /* lat between remaining chunks */2 };

/* memory access bus width (in bytes) */
int mem_bus_width;

/* Array of bus pointers */
struct mem_bank *mem_banks[MAX_NUM_MEMORIES];

#if 0
/* top of the data segment */
md_addr_t mem_brk_point = 0;

/* lowest address accessed on the stack */
md_addr_t mem_stack_min = 0x7fffffff;
#endif

/* first level memory block table */
char *virt_mem_table[VIRT_MEM_TABLE_SIZE];

//char *phys_mem_table[PHYS_MEM_TABLE_SIZE];

/* simulate queuing delay */
int mem_queuing_delay;

/* determines if the memory access is valid, returns error str or NULL */
char *					/* error string, or NULL */
mem_valid(unsigned int cmd,		/* Read (from sim mem) or Write */
	  md_addr_t addr,		/* target address to access */
	  int nbytes,			/* number of bytes to access */
	  int declare)			/* declare the error if detected? */
{
#if 0  
  char *err_str = NULL;

  /* check alignments */
  if ((nbytes & (nbytes-1)) != 0 || (addr & (nbytes-1)) != 0)
    {
      extern int spec_level; 

      err_str = "bad size or alignment";
      /* by zfx */
      if (spec_level) return NULL;
    }
  /* check permissions, no probes allowed into undefined segment regions */
  else if (!(/* text access and a read */
	   (addr >= ld_text_base && addr < (ld_text_base+ld_text_size)
	    && is_read(cmd))
	   /* data access within bounds */
	   || (addr >= ld_data_base && addr < ld_stack_base)))
    {

#if 0
  I am commenting this out temporarily just to debug alpha stuff. REMOVE THE NULL STATEMENT - HRISHI
      err_str = "segmentation violation";
#endif
  	return NULL;
    }

  /* track the minimum SP for memory access stats */
  if (addr > mem_brk_point && addr < mem_stack_min)
    mem_stack_min = addr;

  if (!declare)
    return err_str;
  else if (err_str != NULL)
    fatal(err_str);
  else /* no error */
    return NULL;
#endif
}

/* allocate a memory block */
char *
mem_newblock(void)
{
  /* see misc.c for details on the getcore() function */
  return getcore(MEM_BLOCK_SIZE);
}

/* register memory system-specific options */
void
mem_reg_options(struct opt_odb_t *odb)	/* option data base */
{
  /* none currently */
}

/* check memory system-specific option values */
void
mem_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  /* nada */
}


/* register memory system-specific statistics */
void
mem_reg_stats(struct stat_sdb_t *sdb)
{
#if 0
  stat_reg_uint(sdb, "mem_brk_point",
		"data segment break point",
		(unsigned int *)&mem_brk_point, mem_brk_point, "  0x%08x");
  stat_reg_uint(sdb, "mem_stack_min",
		"lowest address accessed in stack segment",
		(unsigned int *)&mem_stack_min, mem_stack_min, "  0x%08x");

  stat_reg_formula(sdb, "mem_total_data",
		   "total bytes used in init/uninit data segment",
		   "(ld_data_size + 1023) / 1024",
		   "%11.0fk");
  stat_reg_formula(sdb, "mem_total_heap",
		   "total bytes used in program heap segment",
		   "(((mem_brk_point - (ld_data_base + ld_data_size)))+1023)"
		   " / 1024", "%11.0fk");
  stat_reg_formula(sdb, "mem_total_stack",
		   "total bytes used in stack segment",
		   "((ld_stack_base - mem_stack_min) + 1024) / 1024",
		   "%11.0fk");
  stat_reg_formula(sdb, "mem_total_mem",
		   "total bytes used in data, heap, and stack segments",
		   "mem_total_data + mem_total_heap + mem_total_stack",
		   "%11.0fk");
#endif  
}

void mem_bank_reg_stats(struct mem_bank *mp, 
			struct stat_sdb_t *sdb)
{
  char buf[512];
  char *name;

  /* get a name for this cache */
  if (!mp->name || !mp->name[0])
    name = "<unknown>";
  else
    name = mp->name;

  if ((mp->banking_code == GENERIC) || (mp->banking_code == SIMPLE_RAMBUS))
    {
      sprintf(buf, "%s.accesses", name);
      stat_reg_counter(sdb, buf, "total number of accesses", &mp->accesses, 0, NULL);
    }
  else if (mp->banking_code == REAL_RAMBUS)
    {
      /*rambus_timing_reg_stats(mp->rambus, sdb, name);*/
    }
}

/* initialize memory system, call before loader.c */
void
mem_init(void)	/* memory space to initialize */
{
  int i;

  /* initialize the first level physical page table to all empty 
  for (i=0; i<PHYS_MEM_TABLE_SIZE; i++)
    phys_mem_table[i] = NULL;
  */
  
  /* initialize the first level virtual page table to all empty */
  for (i=0; i<VIRT_MEM_TABLE_SIZE; i++)
    virt_mem_table[i] = NULL;
}

extern int is_splash;

/* initialize memory system, call after loader.c */
void
mem_init1(int i)
{
  /* initialize the bottom of heap to top of data segment */
  //cpus[i].mem_brk_point = ROUND_UP(cpus[i].ld_data_base + cpus[i].ld_data_size, MD_PAGE_SIZE);
  if(is_splash)
    cpus[i].ld_brk_point = BRK_START + i * NEW_THREAD_TOTAL_SIZE;
  
  /* set initial minimum stack pointer value to initial stack value */
  //cpus[i].mem_stack_min = cpus[i].regs.regs_R[MD_REG_SP];
}

/* print out memory system configuration */
void
mem_aux_config(FILE *stream)	/* output stream */
{
  /* none currently */
}

/* dump memory system stats */
void
mem_aux_stats(FILE *stream)	/* output stream */
{
  /* zippo */
}

struct mem_bank *
mem_bank_create(char *name)
{
  struct mem_bank *bp;
  bp = (struct mem_bank *) malloc(sizeof(struct mem_bank));
  bp->name = (char *) strdup(name);
  return(bp);
}

tick_t request_bank(tick_t now, 
			  struct mem_bank *mp, 
			  md_addr_t addr, 
			  unsigned int cmd)
{
  tick_t time_diff, response_time, service_time = 0;
  
  /* asserting causality ... since we currently allow double memory
   * operations to occur atomically (TLB miss), even if a bus transaction
   * arrives in the middle, can't enforce causality.  Should schedule the
   * second one to be scheduled at the time it is generated; performance
   * impact should be negligible at the cost of simulation speed */
  /* assert(now >= mp->last_req); */
  /* mp->last_req = now; */

  if (mp->use_paging)
    {
      if ((addr & ~mp->page_cache_mask) == mp->page_cache_tag)
	{
	  /* RDRAM page hit */
	  response_time = service_time = mp->page_hit;
	}
      else
	{
	  /* RDRAM page miss */
	  mp->page_cache_tag = (addr & ~mp->page_cache_mask);
	  response_time = service_time = mp->precharge_on_page_miss + mp->access_time;
	}
    }
  else
    {
      response_time = service_time = mp->access_time;
    }
  
  time_diff = mp->time - now;
  if (mem_queuing_delay && time_diff > 0)
    {
      /* queueing delay */
      mp->time += service_time;
      return(time_diff + response_time);
    }
  else
    {
      /* bus was idle */
      mp->time = now + service_time;
      return(response_time);
    }
}


/* This function is used for switching between the different functions needed
   for different memory bank codes (infinite b/w banks, real rambus model, etc.) */
static inline tick_t
bank_access_switch(tick_t now, 
		   struct mem_bank *mp, 
		   md_addr_t addr, 
		   int cmd, 
		   int nbytes)
{
  tick_t lat = 0;

  /* We subtract one from the rambus cycle count because of the extra bus we had to 
     define (which takes one cycle) to get to the rambus module */
  /*if (mp->banking_code == REAL_RAMBUS)*/
    /*lat = rambus_access(mp->rambus, addr, nbytes, cmd, now) - 1;*/
  /*else*/
  //lat = request_bank(now, mp, addr, Read);
  lat = dram_access_latency(Mem_Read, addr, nbytes, now);
  return(lat);
}

/* Handle memory bank access latencies */
void 
mem_timing_bank_access(tick_t now,
		       struct _cache_access_packet *m_packet)
{
  struct mem_bank *mp = (struct mem_bank *) m_packet->cp;
  md_addr_t pte_addr, addr, baddr = m_packet->addr;
  tick_t lat = 0;
  unsigned int frame;
  md_addr_t page_tbl_entry;
  page_state_u *p;

  /* If we're servicing a TLB miss here  */
  if (is_tlb(m_packet->cmd))
    {
      if (m_packet->vorp == Virtual)
	{
	  lat = tlb_mmu_access(now, MMU_TAG(baddr), &frame);
	  pte_addr = PHYSICAL_PTE(frame, L2_PTE_TAG(baddr));
	  lat += bank_access_switch(now + lat, mp, pte_addr, Mem_Read, m_packet->nbytes);
	}
      else /* if (m_packet->vorp == Physical) */
	{
	  lat = bank_access_switch(now, mp, baddr, Mem_Read, m_packet->nbytes);
	}

      mp->accesses++;
      if (m_packet->release_fn)
	(*(m_packet->release_fn))(now + lat, m_packet->obj, m_packet->stamp);
    }
  else
    {
      if (m_packet->vorp == Virtual)
	{
	  /* No tlb in system, get the frame for the PTE physical address */
	  lat = tlb_mmu_access(now, MMU_TAG(baddr), &frame);

	  /* Calculate the physical address of the page table entry */
	  pte_addr = PHYSICAL_PTE(frame, L2_PTE_TAG(baddr));
	  
	  /* Need one memory probe for the page table entry */
	  lat += bank_access_switch(now + lat, mp, pte_addr, Mem_Read, m_packet->nbytes);
	  /*addr = MAKE_PT_PHYS_ADDRESS(((page_state_u *) 
				       (phys_mem_table[MEM_BLOCK(pte_addr)] + 
				       MEM_OFFSET(pte_addr))), baddr);*/
	  page_tbl_entry = VIRTUAL_PTE(baddr);
 
	  p = ((page_state_u *) (virt_mem_table[MEM_BLOCK(page_tbl_entry)] + 
				 MEM_OFFSET(page_tbl_entry)));
	  
	  assert(IS_INITIALIZED(p));
	  
	  /* Compute the physical address */
	  addr = MAKE_PT_PHYS_ADDRESS(p, MEM_OFFSET(baddr));
	  
	  /* Now need one memory probe for the data */
	  mp->accesses++;
	  lat += bank_access_switch(now + lat, mp, addr, m_packet->cmd, m_packet->nbytes);
	}
      else
	{
	  mp->accesses++;
	  lat = bank_access_switch(now, mp, baddr, m_packet->cmd, m_packet->nbytes);
	}
      if (m_packet->release_fn)
	(*(m_packet->release_fn))(now + lat, m_packet->obj, m_packet->stamp);
    }
}

void 
mem_func_bank_access(tick_t now,
		     cache_access_packet *m_packet)
{
  unsigned int frame;		/* Holds tlb physical frame for MMU access */
  struct mem_bank *mp = (struct mem_bank *) m_packet->cp;

  /* If we're servicing a TLB miss here */
  if (is_tlb(m_packet->cmd))
    {
      if (m_packet->vorp == Virtual)
	{
	  (void) tlb_mmu_access(now, MMU_TAG_FROM_VPTE(m_packet->addr), &frame);
	}
      mp->accesses++;
    }
  else
    {
      if (m_packet->vorp == Virtual)
	{
	  /* These translations are commented out because we only need to actually calculate
	     the physical address here if we need to do timing on interleaving banks */

	  (void) tlb_mmu_access(now, MMU_TAG(m_packet->addr), &frame);
	  /* Uncomment this if we actually need the translation to do banking */
	  /* ptaddr = PHYSICAL_PTE(frame, L2_PTE_TAG(m_packet->baddr));  */
	  mp->accesses++;
	}
      mp->accesses++;
    }
}


