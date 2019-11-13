/*
 * memory.h - flat memory space interfaces
 *
 * Modified by Fuxin Zhang for godson simulator.
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
 *
 */

#ifndef MEM_H
#define MEM_H

#include <stdio.h>
#include "endian.h"
#include "options.h"
#include "stats.h"
#include "mips.h"
/*#include "rambus.h"*/

/* Define memory-related constants */

#define MAX_NUM_MEMORIES 10

/* Banking codes */
#define GENERIC		0
#define SIMPLE_RAMBUS	1
#define REAL_RAMBUS	2

/* Memory commands, was enum mem_cmd, now a series of constants and macros */

#define access_mask 0x00ff
#define command_mask 0xff00

#define Mem_Read  		0x0001
#define Mem_Write 		0x0002
#define Mem_Tlb   		0x0004
#define Mem_Prefetch	0x0008

#define Pipeline_access   0x0100
#define Ifetch_access     0x0200
#define Restarted_access  0x0400
#define Miss_access       0x0800

#define is_read(x)  	((x) & Mem_Read)
#define is_write(x) 	((x) & Mem_Write)
#define is_tlb(x)	((x) & Mem_Tlb)
#define is_pipeline_access(x) 	((x) & Pipeline_access)
#define is_restarted_access(x)	((x) & Restarted_access)
#define is_ifetch_access(x)	((x) & Ifetch_access)
#define is_miss_access(x)	((x) & Miss_access)
#define access_type(x)  ((x) & access_mask)

enum trans_cmd {	/* Sent with addresses to specify whether they are */
  Virtual,		/* virtual or physical addresses */
  Physical
};

struct mem_bank {
  char *name;
  int access_time;
  int banking_code;
  void *request_queue;

  md_addr_t page_cache_tag;
  int page_cache_mask;
  int use_paging;
  int page_hit;
  int precharge_on_page_miss;

  tick_t time;		/* time when bank is free */
  tick_t last_req;	/* time of last request */
  counter_t accesses;

  /* _RAMBUS *rambus; */		/* Pointer to rambus structure, if needed */
};

typedef unsigned long MSHR_STAMP_TYPE;		/* tag an mshr instance */
typedef void (*RELEASE_FN_TYPE)(tick_t now,
				void *obj, MSHR_STAMP_TYPE stamp);
typedef long (*VALID_FN_TYPE)(void *obj, MSHR_STAMP_TYPE stamp);

/* determines if the memory access is valid, returns error str or NULL */
char *					/* error string, or NULL */
mem_valid(unsigned int cmd,		/* Read (from sim mem) or Write */
	  md_addr_t addr,		/* target address to access */
	  int nbytes,			/* number of bytes to access */
	  int declare);			/* declare any detected error? */

/* allocate a memory block */
char *mem_newblock(void);

/* register memory system-specific options */
void
mem_reg_options(struct opt_odb_t *odb);	/* options data base */

/* check memory system-specific option values */
void
mem_check_options(struct opt_odb_t *odb,/* options data base */
		  int argc, char **argv);/* simulator arguments */

/* register memory system-specific statistics */
void
mem_reg_stats(struct stat_sdb_t *sdb);	/* stats data base */

/* initialize memory system */
void mem_init(void);			/* call before loader.c */
void mem_init1(int i);			/* call after loader.c */

/* print out memory system configuration */
void mem_aux_config(FILE *stream);	/* output stream */

/* dump memory system stats */
void mem_aux_stats(FILE *stream);	/* output stream */

/* dump individual bank stats */
void mem_bank_reg_stats(struct mem_bank *mp, 
			struct stat_sdb_t *sdb);
struct mem_bank *
mem_bank_create(char *name);

/* Forward pointer to data type definition */
struct _cache_access_packet;

/* Handle memory bank access latencies */
void 
mem_timing_bank_access(tick_t now,
		       struct _cache_access_packet *m_packet);

void 
mem_func_bank_access(tick_t now,
		     struct _cache_access_packet *m_packet);
/*
 * The SimpleScalar virtual memory address space is 2^31 bytes mapped from
 * 0x00000000 to 0x7fffffff.  The upper 2^31 bytes are currently reserved for
 * future developments.  The address space from 0x00000000 to 0x00400000 is
 * currently used to hold page tables.  The address space from 0x00400000 to
 * 0x10000000 is used to map the program text (code), although accessing any
 * memory outside of the defined program space causes an error to be declared.
 * The address space from 0x10000000 to "mem_brk_point" is used for the program
 * data segment.  This section of the address space is initially set to contain
 * the initialized data segment and then the uninitialized data segment.
 * "mem_brk_point" then grows to higher memory when sbrk() is called to service
 * heap growth.  The data segment can continue to expand until it collides with
 * the stack segment.  The stack segment starts at 0x7fffc000 and grows to
 * lower memory as more stack space is allocated.  Initially, the stack
 * contains program arguments and environment variables (see loader.c for
 * details on initial stack layout).  The stack may continue to expand to lower
 * memory until it collides with the data segment.
 *
 * The SimpleScalar virtual memory address space is implemented with a one
 * level page table, where the first level table contains MEM_TABLE_SIZE
 * pointers to MEM_BLOCK_SIZE byte pages in the second level table.  Pages are
 * allocated in MEM_BLOCK_SIZE size chunks when first accessed, the initial
 * value of page memory is all zero.
 *
 * Graphically, it all looks like this:
 *
 *                 Virtual        Level 1    Host Memory Pages
 *                 Address        Page       (allocated as needed)
 *                 Space          Table
 * 0x00000000    +----------+      +-+      +-------------------+
 *               | pgtable  |      | |----->| memory page (16k)  |
 * 0x00400000    +----------+      +-+      +-------------------+
 *               |          |      | |
 *               | text     |      +-+
 *               |          |      | |
 * 0x10000000    +----------+      +-+
 *               |          |      | |
 *               | data seg |      +-+      +-------------------+
 *               |          |      | |----->| memory page (16k)  |
 * mem_brk_point +----------+      +-+      +-------------------+
 *               |          |      | |
 *               |          |      +-+
 *               |          |      | |
 * regs_R[29]    +----------+      +-+
 * (stack ptr)   |          |      | |
 *               | stack    |      +-+
 *               |          |      | |
 * 0x7fff8000    +----------+      +-+      +-------------------+
 *               | unused   |      | |----->| memory page (16k)  |
 * 0x7fffffff    +----------+      +-+      +-------------------+

 */

/* top of the data segment, sbrk() moves this to higher memory */
//extern md_addr_t mem_brk_point;

/* lowest address accessed on the stack */
//extern md_addr_t mem_stack_min;

/* memory block size, in bytes, also page size */
/* Define two names for the same values, mem_block_size doesn't
 * need to be the page size, but the current implementation assumes
 * it to be that way (e.g. the way virt_mem_table is set up */

//16K blocks
#define PAGE_SIZE		MD_PAGE_SIZE
#define LOG_PAGE_SIZE	        MD_LOG_PAGE_SIZE	

#define MEM_BLOCK_SIZE		MD_PAGE_SIZE
#define LOG_MEM_BLOCK_SIZE	MD_LOG_PAGE_SIZE

/*
 * memory page table defs
 */

/* memory indirect table size (upper mem is not used) */
#define VIRT_MEM_TABLE_SIZE		(1 << ( 32 - LOG_PAGE_SIZE ))

//#define PHYS_MEM_TABLE_SIZE		0x80000

#ifndef HIDE_MEM_TABLE_DEF	/* used by sim-fast.c */
/* the level 1 page table map */
extern char *virt_mem_table[VIRT_MEM_TABLE_SIZE];
//extern char *phys_mem_table[PHYS_MEM_TABLE_SIZE];
#endif /* HIDE_MEM_TABLE_DEF */

extern int mem_nelt;
extern char *mem_configs[MAX_NUM_MEMORIES];
extern int num_mem_banks;
extern int mem_lat[2];;
extern int mem_bus_width;
extern struct mem_bank *mem_banks[MAX_NUM_MEMORIES];

extern int mem_queuing_delay;
/*
 * memory page table accessors
 */

/* Define the macros to extract page frame tags and offsets */
#define MEM_BLOCK(addr) 	((((md_addr_t)(addr)) >> LOG_PAGE_SIZE))
//#define MEM_OFFSET(addr)	((addr) & (MD_PAGE_SIZE - 1))

/* memory tickle function, this version allocates pages when they are touched
   for the first time */

#define MEM_CHECK(addr)                                                 \
  ((MEM_BLOCK(addr) >= VIRT_MEM_TABLE_SIZE) ? panic("Attempting to access virt_mem_table with an index greater than VIRT_MEM_TABLE_SIZE") : 0)

#define __MEM_TICKLE(addr)						\
  (MEM_CHECK(addr), !virt_mem_table[MEM_BLOCK(addr)]			\
   ? (virt_mem_table[MEM_BLOCK(addr)] = tlb_initialize_block(addr))	\
   : 0)

/* fast memory access function, this is not checked so only use this function
   if you are sure that it cannot fault, e.g., instruction fetches, this
   function returns NULL is the memory page is not allocated */
#define __UNCHK_VIRT_MEM_ACCESS(type, addr)					\
  (*((type *)(virt_mem_table[MEM_BLOCK(addr)] + MEM_OFFSET(addr))))

#endif /* MEM_H */


