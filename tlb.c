/*
 * tlb.c - address translation modules
 *
 * This file is a part of the SimpleScalar tool suite, and was 
 * written by Doug Burger, as a part of the Galileo research project.  
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1996, 1997 by Doug Burger
 * Distributed as part of sim-alpha release
 *
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
 *
 */

#include <assert.h>

#include "mips.h"
#include "cache.h"
#include "bus.h"
#include "misc.h"
#include "mem.h"
#include "memory.h"
#include "sim.h"
#include "eventq.h"
#include "tlb.h"


/* Name of level-one data tlb */
char *dtlb_name;

/* Name of level-one instruction tlb */
char *itlb_name;

/* TLB definition counter */
int tlb_nelt = 0;

/* Array of tlb strings */
char *tlb_configs[MAX_NUM_TLBS];

/* Array of tlb pointers */
struct cache *tlbs[MAX_NUM_TLBS];

int num_tlbs;

/* instruction TLB */
struct cache *itlb = NULL;

/* data TLB */
struct cache *dtlb = NULL;

extern void schedule_response_handler(tick_t when,
				      struct mshregisters *msrhp,
				      MSHR_STAMP_TYPE stamp);
extern void
blk_access_fn(tick_t now, 
	      cache_access_packet *c_packet,
	      enum resource_type type);

extern long valid_mshr(struct mshregisters *mshrp, MSHR_STAMP_TYPE stamp);
 /* These arrays hold the physical frame numbers of each level of the page table. The first level is in the MMU*/
 


/* Holds the mmu, which keeps track of the physical addresses of the 
   page table */

static tlb_mmu_type mmu;

/* Holds number of initialized physical page frames, program starts at the */
/* point where the page table ends */
/* Will eventually be supplemented with a free list, etc. */

static int frame_counter = 0; 


void
initialize_mmu_entry(md_addr_t mmu_tag)
{
  /* Virtual space for the page table page should not yet exist, 
   * so we should allocate it */
  assert(!virt_mem_table[mmu_tag]);
  virt_mem_table[mmu_tag] = mem_newblock();

  /* Now initialize the MMU entry */
  mmu.s[mmu_tag].valid = 1;
  mmu.s[mmu_tag].phys_frame = frame_counter;
      
  /* Set the physical memory table to point to the allocated virtual page */
  //phys_mem_table[frame_counter] = virt_mem_table[mmu_tag];

  /* Increment the number of physical frames allocated */
  frame_counter++;
  //assert(frame_counter < PHYS_MEM_TABLE_SIZE);
}

char *
tlb_initialize_block(md_addr_t vaddr)
{
  md_addr_t page_tbl_entry = VIRTUAL_PTE(vaddr);
  md_addr_t mmu_tag = MMU_TAG(vaddr);
  page_state_u *p;

  if (!mmu.s[mmu_tag].valid)
    {
      initialize_mmu_entry(mmu_tag);
    }

  p = ((page_state_u *) (virt_mem_table[MEM_BLOCK(page_tbl_entry)] + 
			 MEM_OFFSET(page_tbl_entry)));

  /* If the page table page but not the page table entry was allocated */
  p->s.valid = 1;
  p->s.owner = 0;		/* We don't currently support more than one node */
  p->s.phys_frame = frame_counter++;

  //assert(frame_counter < PHYS_MEM_TABLE_SIZE);
  //phys_mem_table[p->s.phys_frame] = virt_mem_table[MEM_BLOCK(vaddr)];


  return(mem_newblock());
}

md_addr_t 
tlb_translate_address(md_addr_t va)
{
  md_addr_t page_tbl_entry;
  md_addr_t pa;
  page_state_u *p;


  /* No longer checking permissions here since it's done in the tlb access routines */
  /* The virtual block and the mmu entry should already have been 
   * initialized in functional simulation, if they haven't, it's a bogus 
   * (misspeculated) address */
  if ((!virt_mem_table[MEM_BLOCK(va)]) || (!mmu.s[MMU_TAG(va)].valid))
    return(BAD_ADDRESS);

  page_tbl_entry = VIRTUAL_PTE(va);
 
  p = ((page_state_u *) (virt_mem_table[MEM_BLOCK(page_tbl_entry)] + 
			 MEM_OFFSET(page_tbl_entry)));

  assert(IS_INITIALIZED(p));

  /* Compute the physical address */
  pa = MAKE_PT_PHYS_ADDRESS(p, MEM_OFFSET(va));
  return(pa);
}


void tlb_mmu_init()
{
  int i;

  mmu.access_latency = MMU_ACCESS_LATENCY;
  mmu.free = 0;
  for (i=0; i<MMU_TABLE_ENTRIES; i++)
    {
      mmu.s[i].valid = 0;
      mmu.s[i].phys_frame = 0;
    }
}

/* Returns the latency for the access, plus the physical frame of the page table
 * page in *frame */
tick_t tlb_mmu_access(tick_t now, 
			    unsigned int index, 
			    unsigned int *frame)
{
  int penalty;

  /* The MMU entry should be initialized, unless it's a misspeculated address.
   * This will be an allocation of a page table page for speculative allocation;
   * if that policy changes, may just want to return BAD_ADDRESS here (or 
   * something) instead */
  if (!mmu.s[index].valid)
    {
      initialize_mmu_entry(index);
    }
  //frame_counter++; /* ?! --zfx */
  *frame = mmu.s[index].phys_frame;
  penalty = MMU_ACCESS_LATENCY;

  if (now > mmu.free)
    {
      mmu.free = now + penalty;
      return(penalty);
    }
  else
    {
      mmu.free += penalty;
      return(mmu.free - now);
    }	
}
