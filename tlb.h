/*
 * tlb.h - address translation interfaces
 *
 * This file is a part of the SimpleScalar tool suite, and was 
 * written by Doug Burger, as a part of the Galileo research project.  
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1996, 1997 by Doug Burger
 *
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
 *
 */

#ifndef TLB_H
#define TLB_H
/* ---- Declare constants ---- */

/* Number of bits in the page tag */
/* 32 - LOG_PAGE_SIZE (defined in memory.h */
#define FRAME_SIZE		( 32 - LOG_PAGE_SIZE )
	
/* Number of bits for saving owner (2^OWNER_BITS is max # nodes) */
#define OWNER_BITS		6

/* How many bytes are each address? */

#define ADDRESS_SIZE		4

 
/* 4 bytes per Page Table entry*/
#define PTE_SIZE		4
#define LOG_PTE_SIZE		2
 

/* Define index bits for L2 PTE lookup */
#define L2_PTE_INDEX           (LOG_PAGE_SIZE - LOG_PTE_SIZE) 

 
/* Define size of MMU  hardwired table (for two-level page tables */

/* 
 * Make sure that MMU_TABLE + L2_PTE_INDEX + LOG_PAGE_SIZE == 32 
 */

#define MMU_TABLE 		(32 - L2_PTE_INDEX - LOG_PAGE_SIZE)
#define MMU_TABLE_ENTRIES 	(1<<MMU_TABLE)
#define MMU_ACCESS_LATENCY  	1
#define MMU_INIT_PENALTY       	1

/* Possible values of "valid" field in page table entry */
#define UNINITIALIZED 	0x00
/* #define INVALID 	0x01 */
#define READ_ONLY 	0x10
#define READ_WRITE 	0x11

#define MAX_NUM_TLBS		100

/* ---- Define helpful macros ---- */
//The page table top (addresses 0-PAGE_TABLE_TOP hold the page table)


#define PAGE_TABLE_TOP		(1 << (FRAME_SIZE + LOG_PTE_SIZE))
 

#define MMU_TAG(va)		((va) >> (LOG_PAGE_SIZE + L2_PTE_INDEX))
#define MMU_TAG_FROM_VPTE(vpte)	((vpte) >> (L2_PTE_INDEX + LOG_PTE_SIZE))


#define L2_PTE_TAG(va)	((va >> (LOG_PAGE_SIZE)) & ((1 << L2_PTE_INDEX) - 1))

/* Make physical address of page table page, given a virtual address and 
 * a MMU table entry (or a page table entry) */
#define PHYSICAL_PTE(frame, index)	((frame << LOG_PAGE_SIZE) | (index << LOG_PTE_SIZE))
#define VIRTUAL_PTE(vaddr)		(((vaddr) >> LOG_PAGE_SIZE) << LOG_PTE_SIZE)

/* Make physical address from page table entry and virtual address */
#define MAKE_PT_PHYS_ADDRESS(pt, va)  ((md_addr_t) ((pt->s.phys_frame << LOG_PAGE_SIZE) | MEM_OFFSET(va)))
 
/* Is the page table entry initialized? */
#define IS_INITIALIZED(p)	(((page_state_u *) p)->s.valid)
 
/********************************************************
 * The address translation mechanism looks as follows:
 * 
 * --------------------------------------------------
 * |   Virtual frame (18)       |  Page offset (14) |
 * --------------------------------------------------
 * 
 * On a TLB miss, 
 * 
 * -------------------------------------------------------------
 * | MMU_index (6) |  Physical index (12) |  Page offset (14) |
 * -------------------------------------------------------------
 *         |                     |
 *        \/                     |
 *    MMU_table (2^6 entries)    |
 *         |                     |
 *        \/                     |
 *   PT frame (20 bits)          |
 *         |                     |
 *        \/                     \/
 * ----------------------------------------------------
 * |   PT frame (20)      |  Physical index (10) | 00 |
 * ----------------------------------------------------
 * 
 ********************************************************/

/* ---- Structures ---- */

/* Sum of bits should be 32, bank bits should take up the slack */
/* If we need more than 5 bits for banks, could be trouble */

typedef union
{
  struct state_struct {
    md_addr_t phys_frame: FRAME_SIZE;	/* Holds frame tag */
    unsigned owner : OWNER_BITS;	/* Which processor owns the page */
    unsigned dirty : 1;			/* Is the page dirty */
    unsigned valid : 2;			/* What is the access state of the page */
    unsigned referenced : 2;		/* How much has it been referenced */
  } s;
  unsigned int state;
} page_state_u;

typedef struct mmu_struct {
  tick_t free;			/* When will the resource be free? */
  unsigned int access_latency;		/* How long does it take to access? */
  struct mmu_state {

    md_addr_t phys_frame: FRAME_SIZE;	/* Holds frame tag */

    unsigned valid: 1;			/* Has the MMU entry been initialized? */
  } s[MMU_TABLE_ENTRIES];		/* Want one entry for each page covered by the MMU */
} tlb_mmu_type;


/* ---- Interfaces ---- */

    
char *tlb_initialize_block(md_addr_t va);

md_addr_t tlb_translate_address(md_addr_t va);

tick_t tlb_mmu_access(tick_t now, 
			    unsigned int index, 
			    unsigned int *frame);

extern char *dtlb_name;
extern char *itlb_name;
extern int tlb_nelt;
extern char *tlb_configs[MAX_NUM_TLBS];
extern struct cache *tlbs[MAX_NUM_TLBS];
extern int num_tlbs;
extern struct cache* itlb;
extern struct cache* dtlb;
#endif
