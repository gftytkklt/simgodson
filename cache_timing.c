/*
 * cache_timing.c - cache module routines for timing simulation
 *
 * This file is a part of the SimpleScalar tool suite, originally
 * written by Todd M. Austin as a part of the Multiscalar Research 
 * Project. The file has been rewritten by Doug Burger, as a
 * part of the Galileo research project.  Alain Kagi has also 
 * contributed to this code.
 *
 * Modified by Raj Desikan as part of the Bullseye project
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997 by Todd M. Austin
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "cache.h"
#include "eventq.h"
#include "misc.h"
#include "tlb.h"
#include "bus.h"
#include "stats.h"
#include "bpred.h"
#include "fetch.h"
#include "map.h"
#include "issue.h"
#include "writeback.h"
#include "commit.h"
#include "sim.h"
#include "memory.h"
#include "loader.h"


/* Forward prototypes */



/* Head to list of unused mshr_full_event entries */
static struct mshr_full_event *mshr_full_free_list;

/* Set predefined (but changeable via command-line) maximal
   numbers of various mshr components */
// int regular_mshrs;
// int prefetch_mshrs;
// int mshr_targets;



void
blk_access_fn(tick_t now, 
	      cache_access_packet *c_packet,
	      enum resource_type type)
{
  int lat;

  if (type == Cache)
    {
      lat = cache_timing_access(now, c_packet);
      if (CACHE_HIT(lat) && (c_packet->release_fn))
	{
	  (*(c_packet->release_fn))(now + lat, c_packet->obj, c_packet->stamp);
	}
    }
  else if (type == Memory)
    {
      (void) mem_timing_bank_access(now, c_packet);
      cache_free_access_packet(c_packet);
    }
}

/* create and initialize a general cache structure */
struct cache *					/* pointer to cache created */
cache_timing_create(char *name,			/* name of the cache */
		    int nsets,			/* total number of sets in cache */
		    int bsize,			/* block (line) size of cache */
		    int subblock,		/* subblock factor of cache lines */
		    int balloc,			/* allocate data space for blocks? */
		    int usize,			/* size of user data to alloc w/blks */
		    int assoc,			/* associativity of cache */
		    unsigned int hit_latency,	/* latency in cycles for a hit */
		    enum cache_policy policy,	/* replacement policy w/in sets */
		    enum cache_trans trans,	/* translation addressing scheme */
		    int prefetch,		/* flag to determine if this cache prefetches */
		    int num_resources,		/* number of buses connected under cache */
		    int resource_code,		/* bus selection function index */
		    char *res_names[])		/* Resource name list */
		    
{ 
  struct cache *cp;
  struct cache_blk *blk;
  int i, j, bindex;

  /* check all cache parameters */
  if (nsets <= 0)
    fatal("cache size (in sets) `%d' must be non-zero", nsets);
  if ((nsets & (nsets-1)) != 0)
    fatal("cache size (in sets) `%d' is not a power of two", nsets);
  /* blocks must be at least one datum large, i.e., 8 bytes for SS */
  if (bsize < 4)
    fatal("cache block size (in bytes) `%d' must be 4 or greater", bsize);
  if ((bsize & (bsize-1)) != 0)
    fatal("cache block size (in bytes) `%d' must be a power of two", bsize);
  if (assoc <= 0)
    fatal("cache associativity `%d' must be non-zero and positive", assoc);
  if ((assoc & (assoc-1)) != 0)
    fatal("cache associativity `%d' must be a power of two", assoc);
  if (!(hit_latency > 0))
    fatal("Access latency in cycles must be at least one cycle");
  if (subblock < 0)
    fatal("Subblock parameter must be 0 or greater");
  if (subblock > bsize/8)
    fatal("Degree of subblocking cannot be greater than block size/double word");
    
  /* allocate the cache structure */
  cp = (struct cache *)
    calloc(1, sizeof(struct cache) + (nsets-1)*sizeof(struct cache_set));
  if (!cp)
    fatal("out of virtual memory");

  /* initialize user parameters */
  cp->name = mystrdup(name);
  if(!strcmp(cp->name,"ITLB") || !strcmp(cp->name,"DTLB"))
  {
      cp->is_a_tlb =1;
  }
  else
  {
      cp->is_a_tlb =0;
  }
      
      
  cp->nsets = nsets;
  cp->bsize = bsize;
  cp->assoc = assoc;
  cp->hit_latency = hit_latency;
  cp->policy = policy;
  cp->trans = trans;
  cp->subblock_ratio = subblock;
  

  if (IS_CACHE_SUBBLOCKED(cp))
    {
      cp->sbsize = bsize/subblock;
      cp->subblock_vector_length = (int) ceil((double)cp->subblock_ratio/8);
    }
  else
    {
      cp->sbsize = 0;
      cp->subblock_vector_length = 0;
    }

  cp->num_resources = num_resources;
  cp->resource_code = resource_code;
  for (j=0; j<num_resources; j++)
  {
    cp->resource_names[j] = strdup(res_names[j]);
  }

  /* compute derived parameters */
  cp->hsize = CACHE_HIGHLY_ASSOC(cp) ? (assoc >> 2) : 0;
  cp->blk_mask = bsize-1;
  cp->set_shift = log_base2(bsize);
  cp->set_mask = nsets-1;
  cp->tag_shift = cp->set_shift + log_base2(nsets);
  cp->tag_mask = (1 << (32 - cp->tag_shift))-1;
  cp->tagset_mask = ~cp->blk_mask;

  /* Calculate subblock masks, if necessary */
  if (IS_CACHE_SUBBLOCKED(cp))
    {
      cp->subblock_mask = (cp->bsize/cp->sbsize) - 1;
      cp->subblock_shift = log_base2(cp->sbsize);
    }

  /* print derived parameters during debug */
  debug("%s: cp->blk_mask  = 0x%08x", cp->blk_mask);
  debug("%s: cp->set_shift = %d", cp->set_shift);
  debug("%s: cp->set_mask  = 0x%08x", cp->set_mask);
  debug("%s: cp->tag_shift = %d", cp->tag_shift);
  debug("%s: cp->tag_mask  = 0x%08x", cp->tag_mask);

  /* initialize cache stats */
  cp->hits = 0;
  cp->misses = 0;
  cp->replacements = 0;
  cp->writebacks = 0;
  cp->invalidations = 0;
  cp->read_traffic = 0;
  cp->write_traffic = 0;
  cp->address_traffic = 0;
  cp->mshr_full = 0;
  if (IS_CACHE_SUBBLOCKED(cp))
    {
      cp->partial_misses = 0;
    }

  /* blow away the last block accessed */
  /* Set it to a value too big for any tag */
  cp->last_tagset = 0xffffffff;
  cp->last_blk = NULL;

  /* allocate data blocks */
  cp->data = (char *)calloc(nsets * assoc,
			    sizeof(struct cache_blk) +
			    (cp->balloc ? (bsize*sizeof(char)) : 0));
  if (!cp->data)
    fatal("out of virtual memory");

  /* slice up the data blocks */
  for (bindex=0,i=0; i<nsets; i++)
    {
      cp->sets[i].way_head = NULL;
      cp->sets[i].way_tail = NULL;
      /* get a hash table, if needed */
      if (cp->hsize)
	{
	  cp->sets[i].hash =
	    (struct cache_blk **)calloc(cp->hsize, sizeof(struct cache_blk *));
	  if (!cp->sets[i].hash)
	    fatal("out of virtual memory");
	}

      cp->sets[i].blks = CACHE_BINDEX(cp, cp->data, bindex);
      
      /* link in the data blocks */
      for (j=0; j<assoc; j++)
	{
	  /* locate next cache block */
	  blk = CACHE_BINDEX(cp, cp->data, bindex);
	  bindex++;

	  /* invalidate new cache block */
	  blk->status = INVALID;

	  /* Setting the tag to j is just to prevent long chains in the hash 
	     table; won't matter because the block is invalid */
	  blk->tag = j;
	  blk->ready = 0;
	  blk->blk_no = j;

	  /* insert cache block into set hash table */
	  if (cp->hsize)
	    link_htab_ent(cp, &cp->sets[i], blk);

	  /* insert into head of way list, order is arbitrary at this point */
	  blk->way_next = cp->sets[i].way_head;
	  blk->way_prev = NULL;
	  if (cp->sets[i].way_head)
	    cp->sets[i].way_head->way_prev = blk;
	  cp->sets[i].way_head = blk;
	  if (!cp->sets[i].way_tail)
	    cp->sets[i].way_tail = blk;
	}
    }

  MSHR_NREGULARS(cp) = 0;
  MSHR_MAXREGULARS(cp) = 0;
  MSHR_NPREFETCH(cp) = 0;
  for (i = 0; i < MAX_MSHRS; i++)
    {
      MSHR_CP(cp, i) = cp;
      MSHR_ADDR(cp, i) = UNUSED_MSHREGISTER;
      MSHR_STAMP(cp, i) = 0;
      if (i >= regular_mshrs)
	MSHR_PREFETCH(cp, i) = TRUE;
      else
	MSHR_PREFETCH(cp, i) = FALSE;
    }
  cp->mshr_hits = 0;
  cp->mshr_misses = 0;

  cp->mshr_full_head = NULL;
  cp->mshr_full_tail = NULL;

  cp->prefetch = prefetch;
  cp->prefetch_out_of_bound = 0;
  cp->prefetch_in_cache = 0;
  cp->prefetch_requested = 0;
  cp->prefetch_full = 0;
  cp->prefetch_crosses_page_boundary = 0;
#if 0
  cp->inst_mask = (bsize/(line_pred_width*sizeof(md_inst_t)))-1;
  cp->inst_shift = log_base2(sizeof(md_inst_t))+log_base2(line_pred_width);
  if(!strcmp(name, icache_name) && way_pred_latency){
    int nblks = assoc;
    int noffset = bsize/(fetch_width*sizeof(md_inst_t));
    cp->way_pred_latency = way_pred_latency;
    cp->way_pred_table = (int ***) calloc(nsets, sizeof (int **));
    for (i=0;i<nsets;i++) {
      cp->way_pred_table[i] = (int **)calloc(nblks, sizeof (int *));
      for (j=0; j<nblks;j++)
	cp->way_pred_table[i][j] = (int *)calloc(noffset, sizeof (int));
    }
  } 
  else
    cp->way_pred_latency = 0;
  if (!strcmp(name, icache_name) && line_predictor) {
   int nblks = assoc;
   int noffset = bsize/(line_pred_width*sizeof(md_inst_t));
   if (bsize < (line_pred_width*sizeof(md_inst_t)))
     fatal("Line predictor crosses cache blk boundary");
   cp->line_predictor = line_predictor;
   cp->line_pred_table = (struct line_pred_struct ***) calloc(nsets, sizeof (struct line_pred_struct **));
   for (i=0;i<nsets;i++) {
     cp->line_pred_table[i] = (struct line_pred_struct **)calloc(nblks, sizeof (struct line_pred_struct *));
     for (j=0; j<nblks;j++)
       cp->line_pred_table[i][j] = (struct line_pred_struct *)calloc(noffset, sizeof (struct line_pred_struct));
   } 
  }
  else
    cp->line_predictor = FALSE;
#endif
  if ((!strcmp(name, icache_name) || !strcmp(name, dcache_name)) && 
      victim_buf_lat) {
    cp->victim_buffer = victim_buf_lat;
  }
  else
    cp->victim_buffer = 0;
  
  return cp;
}

/* register cache stats */
void
cache_timing_reg_stats(struct cache *cp,	/* cache instance */
		       struct stat_sdb_t *sdb)	/* stats database */
{
  char buf[512], buf1[512], *name; 

  /* get a name for this cache */
  if (!cp->name || !cp->name[0])
    name = "<unknown>";
  else
    name = cp->name;

  sprintf(buf, "%s.accesses", name);
  sprintf(buf1, "%s.hits + %s.misses", name, name);
  stat_reg_formula(sdb, buf, "total number of accesses", buf1, NULL);
  sprintf(buf, "%s.hits", name);
  stat_reg_counter(sdb, buf, "total number of (all) hits", &cp->hits, 0, NULL);

  sprintf(buf, "%s.misses", name);
  stat_reg_counter(sdb, buf, "total number of misses", &cp->misses, 0, NULL);

  sprintf(buf, "%s.mshr_misses", name);
  stat_reg_counter(sdb, buf, "total number of misses to mshrs", &cp->mshr_misses, 0, NULL);
  sprintf(buf, "%s.mshr_full", name);
  stat_reg_counter(sdb, buf, "total number of times mshr was full", &cp->mshr_full, 0, NULL);
  if (IS_CACHE_SUBBLOCKED(cp))
    {
      sprintf(buf, "%s.partialmisses", name);
      stat_reg_counter(sdb, buf, "misses to transfer blocks", &cp->partial_misses, 0, NULL);

      sprintf(buf, "%s.blockmisses", name);
      sprintf(buf1, "%s.misses - %s.partialmisses", name, name);
      stat_reg_formula(sdb, buf, "misses to address blocks", buf1, NULL);
    }

  sprintf(buf, "%s.replacements", name);
  stat_reg_counter(sdb, buf, "total number of replacements",
		 &cp->replacements, 0, NULL);

  sprintf(buf, "%s.writebacks", name);
  stat_reg_counter(sdb, buf, "total number of writebacks",
		 &cp->writebacks, 0, NULL);

  sprintf(buf, "%s.read_traffic", name);
  stat_reg_counter(sdb, buf, "total number of bytes read", &cp->read_traffic, 0, NULL);

  sprintf(buf, "%s.write_traffic", name);
  stat_reg_counter(sdb, buf, "total number of bytes written back", &cp->write_traffic, 0, NULL);

  sprintf(buf, "%s.address_traffic", name);
  stat_reg_counter(sdb, buf, "total number of bytes transmitted for tags", &cp->address_traffic, 0, NULL);

  sprintf(buf, "%s.traffic", name);
  sprintf(buf1, "%s.read_traffic + %s.write_traffic", name, name);
  stat_reg_formula(sdb, buf, "total number of r/w bytes transmitted", buf1, NULL);

  sprintf(buf, "%s.invalidations", name);
  stat_reg_counter(sdb, buf, "total number of invalidations",
		 &cp->invalidations, 0, NULL);

  sprintf(buf, "%s.miss_rate", name);
  sprintf(buf1, "%s.misses / %s.accesses", name, name);
  stat_reg_formula(sdb, buf, "miss rate (i.e., misses/ref)", buf1, NULL);

  sprintf(buf, "%s.puremiss_rate", name);
  sprintf(buf1, "(%s.mshr_misses)/ %s.accesses", name, name);
  stat_reg_formula(sdb, buf, "non-mshr-hit miss rate (i.e., mshr_misses/ref)", buf1, NULL);

  sprintf(buf, "%s.repl_rate", name);
  sprintf(buf1, "%s.replacements / %s.accesses", name, name);
  stat_reg_formula(sdb, buf, "replacement rate (i.e., repls/ref)", buf1, NULL);

  sprintf(buf, "%s.wb_rate", name);
  sprintf(buf1, "%s.writebacks / %s.accesses", name, name);
  stat_reg_formula(sdb, buf, "writeback rate (i.e., wrbks/ref)", buf1, NULL);

  sprintf(buf, "%s.inv_rate", name);
  sprintf(buf1, "%s.invalidations / %s.accesses", name, name);
  stat_reg_formula(sdb, buf, "invalidation rate (i.e., invs/ref)", buf1, NULL);
#if 0
  if (cp->way_pred_latency) {
    sprintf(buf, "%s.way_pred_lookups", name);
    stat_reg_counter(sdb, buf, "Number of way predictor lookups", &cp->way_pred_lookups, 0, NULL);
    sprintf(buf, "%s.way_pred_hits", name);
    stat_reg_counter(sdb, buf, "Number of way predictor hits", &cp->way_pred_hits, 0, NULL);
    sprintf(buf, "%s.way_pred_misses", name);
    stat_reg_counter(sdb, buf, "Number of way predictor misses", &cp->way_pred_misses, 0, NULL);
  }
  if (cp->line_predictor) {
    sprintf(buf, "%s.line_pred_lookups", name);
    stat_reg_counter(sdb, buf, "Number of line predictor lookups", &cp->line_pred_lookups, 0, NULL);
     sprintf(buf, "%s.line_pred_hits", name);
    stat_reg_counter(sdb, buf, "Number of line predictor hits", &cp->line_pred_hits, 0, NULL);
    sprintf(buf, "%s.line_pred_misses", name);
    stat_reg_counter(sdb, buf, "Number of line predictor misses", &cp->line_pred_misses, 0, NULL);
  }
#endif
  if (cp->prefetch)
    {
      sprintf(buf, "%s.prefetches", name);
      stat_reg_counter(sdb, buf, "prefetch issued", &cp->prefetch_issued, 0, NULL);
      sprintf(buf, "%s.p_illegal", name);
      stat_reg_counter(sdb, buf, "illegal prefetch", &cp->prefetch_out_of_bound, 0, NULL);
      sprintf(buf, "%s.p_in_cache", name);
      stat_reg_counter(sdb, buf, "prefetch already in cache", &cp->prefetch_in_cache, 0, NULL);
      sprintf(buf, "%s.p_requested", name);
      stat_reg_counter(sdb, buf, "prefetch, data already requested", &cp->prefetch_requested, 0, NULL);
      sprintf(buf, "%s.p_no_mshrs", name);
      stat_reg_counter(sdb, buf, "prefetch, mshr table full", &cp->prefetch_full, 0, NULL);
      sprintf(buf, "%s.p_page_boundary", name);
      stat_reg_counter(sdb, buf, "page boundary crossed", &cp->prefetch_crosses_page_boundary, 0, NULL);
    }
}

void schedule_response_handler(tick_t when,
				      struct mshregisters *msrhp,
				      MSHR_STAMP_TYPE stamp);
              
static void response_handler(tick_t now,
			     struct mshregisters *mshrp,
			     MSHR_STAMP_TYPE stamp);
static int allocate_regular_mshr_register(struct cache *cp);
static int allocate_target(struct cache *cp, int which_register, md_addr_t addr, unsigned int cmd);

long valid_mshr(struct mshregisters *mshrp, MSHR_STAMP_TYPE stamp);

/* attempts to find mshr entry matching ADDR, if found goto HIT_LABEL
   (and ENTRY records the matching entry number), fall through otherwise. 
   If the block is subblocked and the appropriate valid bit isn't set
   in the vector for which subblocked have been requested, continue searching */
#define FIND_MSHR_MATCH(CP, ENTRY, ADDR, SB_BIT, IS_SUBBLOCK_MSHR_MISS, HIT_LABEL)		\
	for ((ENTRY) = 0; (ENTRY) <= MSHR_MAXREGULARS(cp); (ENTRY)++)				{\
	  if (MSHR_ADDR((CP), (ENTRY)) == (ADDR))						{\
            if (MSHR_SUBBLOCK_VECTOR((CP), (ENTRY)) && (!(CACHE_GET_SB_BIT(SB_BIT, MSHR_SUBBLOCK_VECTOR((CP), (ENTRY))))))	{\
              IS_SUBBLOCK_MSHR_MISS = 1;							}\
            else										\
              goto HIT_LABEL ;}}

#if 0
static int
check_address(md_addr_t addr, 
	      unsigned int cmd, 
	      int nbytes, 
	      md_addr_t blk_mask, 
	      int bsize)
{
  if (is_tlb(cmd))
    {
      if (!((addr >= 0) && (addr < PAGE_TABLE_TOP)))
	return(FALSE);
    }
  else
    {
      if (!((addr >= ld_text_base && addr < (ld_text_base+ld_text_size)
	     && (is_read(cmd)))
	    || (addr >= ld_data_base && addr < ld_stack_base)
	    || ((addr < PAGE_TABLE_TOP) && (is_tlb(cmd)))
	    || ((virt_mem_table[MEM_BLOCK(addr)] != NULL) && (addr > ld_text_base))))
	{
	  return(FALSE);
	}
      if ((nbytes & (nbytes-1)) != 0 || (addr & (nbytes-1)) != 0)
	{
	  return(FALSE);
	  /*    fatal("cache: access error: bad size or alignment, addr 0x%08x", addr); */
	}
      /* access must fit in cache block */
      if ((addr + nbytes) > ((addr & ~blk_mask) + bsize))
	{
	  return(FALSE);
	  /*    fatal("cache: access error: access spans block, addr 0x%08x", addr); */
	}
    }
  return(TRUE);
}
#endif

static void 
do_writeback(tick_t now, 
	     struct cache *cp,
	     md_addr_t baddr,
	     enum trans_cmd vorp,
	     struct cache_blk *blk)
{
  void *next_mp;
  struct bus *wb_bus;
  enum resource_type type;
  tick_t delay;
  int size;
  cache_access_packet *c_packet;

  /* Find out which bus to use */
  wb_bus = bus_select(cp->num_resources, cp->resource_code, cp->resources);
  next_mp = bus_follow(wb_bus, &type);

  /* Extra penalty to read the block out for writing */
  delay = cp->hit_latency;

  /* write back the cache block */
  cp->writebacks++;

  if (ARE_SUBBLOCKS_DIRTY(blk))
    {
      /* Figure out the number of subblocks to send down */
      size = cp->sbsize * count_valid_bits(cp, blk->sb_dirty);
      cp->address_traffic += cp->subblock_vector_length;
    }
  else
    {
      size = cp->bsize;
    }

  cp->write_traffic += size;
  cp->address_traffic += ADDRESS_SIZE;

  /* latency, request sent down the bus */
  delay += bus_access(now, size, wb_bus, Write_Request);

  /* write back */
  c_packet = cache_create_access_packet(next_mp, (Mem_Write | Miss_access), baddr, vorp, 
					size, NULL, NULL, NULL, 0);

  eventq_queue_callback2(now + delay, (void *) blk_access_fn, (int) c_packet, type);

}


/* implements tagged prefetch, see Cache Memories, A. J. Smith, 1994 */
/* Assumed that address sent is block address plus block size */
static void
do_prefetch(tick_t now, 
	    struct cache *cp, 
	    md_addr_t addr, 
	    enum trans_cmd vorp, 
	    md_addr_t c_set)
{
#if 0
  int mshr_index, lat = 0, size, subblock_miss = FALSE;
  md_addr_t set;
  md_addr_t tag;
  md_addr_t sb;
  struct cache_blk *blk;
  struct bus *bus;
  enum resource_type type;
  void *next_mp;
  cache_access_packet *c_packet;
  tag = CACHE_TAG(cp, addr);

  /* Assumes that next block to prefetch will be the set incremented by one (i.e. the
     set index bits are the low-order bits above the block offset */
  set = CACHE_SET_INC(cp, c_set);
  sb = CACHE_SB_TAG(cp, addr);

  /* check alignment */
  if (((addr & (cp->bsize - 1)) != 0) && ((!IS_CACHE_SUBBLOCKED(cp)) || ((addr & (cp->sbsize - 1)) != 0)))
    fatal("do_prefetch: access error: bad alignment, addr %#0x", addr);

  if ((CACHE_BADDR(cp, addr) != addr) && ((!IS_CACHE_SUBBLOCKED(cp)) || (CACHE_SBADDR(cp, addr) != addr)))
    fatal("do_prefetch: bad prefetch addr, addr %#0x", addr);

  if (MEM_BLOCK(addr) != MEM_BLOCK(addr - cp->bsize))
    {
      cp->prefetch_crosses_page_boundary++;
      return;
    }

  if ((cp->trans == VIVT) && (addr < ld_text_base || addr >= ld_text_base + ld_text_size))
    {
      cp->prefetch_out_of_bound++;
      return;
    }

  /* low-associativity cache, linear search the way list */
  FIND_BLK_MATCH(cp, blk, set, tag, sb, subblock_miss, cache_hit)
  
  /* already requested? */
  FIND_MSHR_MATCH(cp, mshr_index, addr, sb, subblock_miss, mshr_hit)

  /* not already requested! room for more? */
  if (cp->nprefetches < prefetch_mshrs)
    {
      for (mshr_index = regular_mshrs; mshr_index < (regular_mshrs+prefetch_mshrs); mshr_index++)
	if (MSHR_ADDR(cp, mshr_index) == UNUSED_MSHREGISTER)
	  break;
    }
  else
    {
      cp->prefetch_full++;
      return;
    }

  cp->prefetch_issued++;

  cp->nprefetches++;

  if ((subblock_miss) || (FETCH_SUBBLOCK(cp, blk, addr)))
    {
      cp->address_traffic += cp->subblock_vector_length;
      MSHR_SUBBLOCK_VECTOR(cp, mshr_index) = CREATE_SUBBLOCK_FETCH_VECTOR(cp, blk, sb);
      size = cp->sbsize * count_valid_bits(cp, MSHR_SUBBLOCK_VECTOR(cp, mshr_index));
    }
  else
    {
      size = cp->bsize;
    }

  cp->address_traffic += ADDRESS_SIZE;
  /* Is this c_packet required???? Raj */
  c_packet = cache_create_access_packet(cp, (Mem_Read | Mem_Prefetch), addr, vorp, 
					size, NULL, NULL, NULL, 0);

  bus = bus_select(cp->num_resources, cp->resource_code, cp->resources);

  MSHR_ADDR(cp, mshr_index) = addr;
  MSHR_CMD(cp, mshr_index) = Mem_Read | Mem_Prefetch;
  MSHR_BUS(cp, mshr_index) = bus;
  MSHR_SIZE(cp, mshr_index) = size;
  if (cp->trans == VIPT)
    {
      MSHR_VSET(cp, mshr_index) = c_set;
    }
  MSHR_INIT_TARGET(cp, mshr_index, /* target_index */0, now, c_packet);
  MSHR_NTARGETS(cp, mshr_index) = 1;
  next_mp = bus_follow(bus, &type);

  lat += bus_access(now, ADDRESS_SIZE, bus, Read_Request);
  /* Note that we prefetch whole address blocks here; should this be the case for subblocking? */
  /* schedule access to the lower level of the mem. hierarchy */
  c_packet = cache_create_access_packet(next_mp, (Read | Miss_access), addr, vorp, 
					size, (void *)&cp->mshregisters[mshr_index],
					(RELEASE_FN_TYPE) schedule_response_handler,
					(VALID_FN_TYPE) valid_mshr,
					(MSHR_STAMP_TYPE) MSHR_STAMP(cp, mshr_index));

  eventq_queue_callback2(now + lat, (void *) blk_access_fn, (int) c_packet, type);

  return;

 cache_hit:

  cp->prefetch_in_cache++;
  return;

 mshr_hit:

  cp->prefetch_requested++;
  return;
#endif
}


/* Restart the cache access that was blocked, due to a TLB miss */
static void 
restart_cache_access(tick_t now, 
		     cache_access_packet *c_packet)
{
  int lat;

  c_packet->cmd |= Restarted_access;
#if 0
  if (c_packet->addr == 0 && (c_packet->release_fn != NULL)) {
    if ((c_packet->valid_fn == NULL) || (c_packet->valid_fn(c_packet->obj, c_packet->stamp)))
      c_packet->release_fn(now+1, c_packet->obj, c_packet->stamp);
    return;
  }
#endif    
  lat = cache_timing_access(now, c_packet);

  /* If it was a cache hit, restart the operation at the appropriate time, 
     because cache_access won't do the callback on a cache hit */
  if (CACHE_HIT(lat) && (c_packet->release_fn != NULL))
    {
      if ((c_packet->valid_fn == NULL) || (c_packet->valid_fn(c_packet->obj, c_packet->stamp)))
	  c_packet->release_fn(now+lat, c_packet->obj, c_packet->stamp);
    }
  //  breaks at simcycle = 1150860709
  if (lat == BAD_ADDRESS) {
    fprintf(stderr, "Warning: Bad address returned from cache_access() in restart_cache_access.\n");

    if (c_packet->release_fn != NULL) {
      if ((c_packet->valid_fn == NULL) || (c_packet->valid_fn(c_packet->obj, c_packet->stamp)))
	c_packet->release_fn(now+1, c_packet->obj, c_packet->stamp);
    }
  }
  /* The cache hierarchy will take care of the packet (freeing it, etc.) from	
     here, in the event of a hit or a miss */
}

static struct mshr_full_event *
get_mshr_full_event()
{
  int i;
  struct mshr_full_event *temp;

  if (mshr_full_free_list == NULL)
    {
      for (i=0; i<CACHE_ACCESS_PACKET_FREE_LIST; i++)
	{
	  temp = (struct mshr_full_event *) malloc(sizeof(struct mshr_full_event));
	  temp->next = mshr_full_free_list;
	  mshr_full_free_list = temp;
	}
    }
  assert(mshr_full_free_list != NULL);
  temp = mshr_full_free_list;
  mshr_full_free_list = temp->next;
  temp->next = NULL;
  return(temp);
}

static inline struct mshr_full_event *
get_next_mshr_full_event(struct cache *cp)
{
  struct mshr_full_event *temp;

  temp = cp->mshr_full_head;
  cp->mshr_full_head = cp->mshr_full_head->next;
  if (cp->mshr_full_head == NULL)
    {
      cp->mshr_full_tail = NULL;
    }
  temp->next = NULL;
  return(temp);
}

static void
free_mshr_full_event(struct mshr_full_event *freeme)
{
  freeme->next = mshr_full_free_list;
  mshr_full_free_list = freeme;
}

static void 
process_mshr_full_event(tick_t now, struct cache *cp)
{
  struct mshr_full_event *temp;
  cache_access_packet *c_packet;
  int i;

  if (cp->nregulars < regular_mshrs)
    {
      for (i=0; ((i < MSHR_FULL_EVENTS_PER_CYCLE) && (cp->mshr_full_head)); i++)
	{
	  temp = get_next_mshr_full_event(cp);
	  c_packet = temp->c_packet;
	  
	  if (is_ifetch_access(c_packet->cmd))
	    {
	      fetch_resume_ifetch(now, MSHR_STALL);
	      cache_free_access_packet(c_packet);
	    }
	  else if (is_pipeline_access(c_packet->cmd))
	    {
	      cache_unblock_memop(now, c_packet);
	    }
	  /* If we have full MSHRs for the TLB, don't want to flood the system with speculative
	   * TLB misses, so check to see if this is an L1 TLB access, and if the request has already
	   * been squashed, free the resources and drop the TLB and cache accesses */
	  /* (so this part of the conditional could be axed for elegance at a performance loss) */
	  else if (is_tlb(c_packet->cmd) && (!is_miss_access(c_packet->cmd)))
	    {
	      cache_access_packet *cache_packet = (cache_access_packet *) c_packet->obj;
	      
	      /* If the original memory operation has not been squashed, restart the tlb access */
	      if ((!cache_packet->valid_fn) || (cache_packet->valid_fn(cache_packet->obj, cache_packet->stamp)))
		restart_cache_access(now, c_packet);
	      /* If the original operation was squashed, free both the tlb access and the buffered
		 cache access */
	      else
		{
		  cache_free_access_packet(cache_packet);
		  cache_free_access_packet(c_packet);
		}
	    }
	  else 
	    {
	      restart_cache_access(now, c_packet);
	    }
	  free_mshr_full_event(temp);
	}
      
      /* If we still have some mshr_full events to process, try a number of them next cycle */
      if ((cp->mshr_full_head) && (cp->nregulars < regular_mshrs))
	{
	  eventq_queue_callback(now+1, (void *)process_mshr_full_event, (int)cp);
	}
    }
}

static void
process_blocked_mshr(cache_access_packet *c_packet)
{
  struct mshr_full_event *temp;
  struct cache *cp = c_packet->cp;

  temp = get_mshr_full_event();
  temp->c_packet = c_packet;
  temp->next = NULL;

  if (cp->mshr_full_head == NULL)
    {
      cp->mshr_full_head = temp;
      cp->mshr_full_tail = temp;
    }
  else
    {
      cp->mshr_full_tail->next = temp;
      cp->mshr_full_tail = temp;
    }
}

/* Schedule the handler that restarts a cache access after a TLB miss */
void schedule_restart_cache_access(tick_t when, cache_access_packet *c_packet, 
				   MSHR_STAMP_TYPE stamp)
{
    eventq_queue_callback(when, (void *)restart_cache_access, (int)c_packet);
}


#if 0
void 
cache_load_store_exec(tick_t when, struct load_store_queue *lqsq, 
		      MSHR_STAMP_TYPE stamp) {
  static struct rqueue_link rq_link;
  static struct queue_elem elem;
  assert (lqsq != NULL && (lqsq->type == SQ || lqsq->type == LQ));
  if (lqsq->type == SQ && lqsq->tag == stamp) {
    if (lqsq->cachemiss == TRUE) {
      /*assert(commit_sq_num > 0);
      commit_sq_num--;
      commit_sq_head = (commit_sq_head + 1) % commit_sq_nelem; */
    } 
    else {
      map_rb[lqsq->inst_desc->r_buf_no].completed = TRUE;
    }
  }
  else if (lqsq->type == LQ && lqsq->tag == stamp) {
    elem.inst_desc = lqsq->inst_desc;
    rq_link.qelem = &elem;
    rq_link.qelem->issued = TRUE;
    rq_link.qelem->op_ready[0] = rq_link.qelem->op_ready[1] = TRUE;
    queue_event(&rq_link, sim_cycle+1);
  } 
}
#endif

void 
cache_unblock_memop(tick_t when, cache_access_packet *c_packet)
{
#if 0
  static struct load_store_queue *lqsq;
  static struct rqueue_link rq_link;
  static struct queue_elem elem;
  if (c_packet->obj != NULL && c_packet->valid_fn != NULL &&  
      c_packet->valid_fn(c_packet->obj, c_packet->stamp)) {
    lqsq = (struct load_store_queue *)c_packet->obj;
    assert (lqsq->type == LQ || lqsq->type == SQ);
    if (lqsq->type == SQ) {
      map_rb[lqsq->inst_desc->r_buf_no].completed = TRUE;
    }
    else {
      elem.inst_desc = lqsq->inst_desc;
      rq_link.qelem = &elem;
      rq_link.qelem->issued = TRUE;
      rq_link.qelem->op_ready[0] = rq_link.qelem->op_ready[1] = TRUE;
      queue_event(&rq_link, sim_cycle+1);
    }
  }
  cache_free_access_packet(c_packet);
#endif
}

/* When a store has been blocked from committing in the LSQ due to a 
   cache miss, TLB miss, or full MSHRS, or a load has been blocked from
   issuing due to a TLB miss or full MSHRS at the L1 dcache, this function 
   allows it to continue by unblocking it */
void 
schedule_cache_unblock_memop(tick_t when, unsigned int ptr, 
			     MSHR_STAMP_TYPE null_stamp)
{
  eventq_queue_callback(when, (void *)cache_unblock_memop, (int) ptr);
}

static int
cache_translate_address(tick_t now,
			cache_access_packet *c_packet,
			md_addr_t vaddr)
{
  struct cache *cp = c_packet->cp;
  unsigned int cmd = c_packet->cmd;
  md_addr_t ptaddr;
  tick_t lat = 0, tlb_lat = 0, mmu_lat = 0;
  unsigned int frame;		/* Holds tlb physical frame for MMU access */
  struct bus *bus;
  enum resource_type type;
  void *next_mp;
  void *callback = NULL;
  cache_access_packet *tlb_packet;

  if (!(is_tlb(cmd))) {
      /* If the restarted_access flag in the cmd field is 0, we are doing a TLB lookup 
       * to get the translation (if it's 1, that means that the access has been restarted 
       * after a TLB miss.  If it is a speculative access, just return the translation
       * (our policy decision is not to take a TLB miss on a misspeculation; this adds
       * a bit of error, since a real implementation would wait until the speculative state
       * was resolved before processing a TLB miss, but we'll just fake it and assume a TLB
       * hit, which will send a memory request to a misspeculated TLB miss access down the
       * memory hierarchy, unlike a real machine.  Since TLB misses are infrequent, this
       * should be a small error */

      if (!(is_restarted_access(cmd))) {
	  if (is_pipeline_access(cmd)) {
	      callback = (void *)schedule_cache_unblock_memop;
	    }
	  else if (is_ifetch_access(cmd)) {
	      callback = (void *)schedule_tlb_resume_ifetch;
	    }
	  else if (is_miss_access(cmd)) {
	      callback = (void *)schedule_restart_cache_access;
	    }
	  assert(callback != NULL);

	  /* If we have a TLB, then do the lookup and take the appropriate action */
	  if (cp->tlb) {
	      ptaddr = VIRTUAL_PTE(vaddr);

	      tlb_packet = cache_create_access_packet(cp->tlb, Mem_Tlb, ptaddr, Virtual, 
				      PTE_SIZE, c_packet, callback, NULL, 0);
	      tlb_lat = cache_timing_access(now, tlb_packet);

	      if (!CACHE_HIT(tlb_lat)) 	/* This includes CACHE_MISS, TARGETS_FULL, and MSHRS_FULL */{
		  /* We just return TLB_MISS here, rather than tlb_lat, since the caller
		     doesn't need to know why the TLB is stalled, only that it is. */
		  return(TLB_MISS);
		}
	    }
	  /* If there is no tlb, look up in the MMU and send a request for the
	   * translation's physical address to the next level of the hierarchy */
	  else {
	      mmu_lat = tlb_mmu_access(now, MMU_TAG(vaddr), &frame);
	      ptaddr = PHYSICAL_PTE(frame, L2_PTE_TAG(vaddr)); 
	      bus = bus_select(cp->num_resources, cp->resource_code, cp->resources);
	      lat += bus_access(now, PTE_SIZE, bus, Read_Request);
	      next_mp = bus_follow(bus, &type);
	      tlb_packet = cache_create_access_packet(next_mp, (Mem_Tlb | Miss_access),
						      ptaddr, Physical, PTE_SIZE,
						      c_packet, callback, NULL, 0);

	      eventq_queue_callback2(now + mmu_lat + lat, (void *) blk_access_fn, (int) tlb_packet, type);
	      return TLB_MISS;
	    }
	}
      /* We are restarting, so assume tlb hit */
      else /* if is_restarted_access(cmd) */ {
	  assert(cp->tlb);	/* If there's no tlb, shouldn't be restarting
				   after a tlb miss! */
	  tlb_lat = cp->tlb->hit_latency;
	}

      /* On a TLB hit, or if restart is non-null (restarting the cache access), 
       * we'll need to get the physical address */

      /* Gets here on a TLB hit or a restarted access, which is an implicit TLB hit */
      c_packet->addr = tlb_translate_address(vaddr);
  }
  else
    {
      /* This is where we access the MMU */
      /* Address translation for TLB entry: the vaddr here is the virtual PTE 
       * address, so we need to just extract the MMU tag */
      tlb_lat = tlb_mmu_access(now, MMU_TAG_FROM_VPTE(vaddr), &frame);
      c_packet->addr = PHYSICAL_PTE(frame, L2_PTE_TAG(vaddr));
      c_packet->vorp = Physical;
    }

  return(tlb_lat);
}

/* does a cache lookup.  returns a non-positive integer if access latency 
 is unknown at the time of the call (could be one of several codes, like
 miss, tlb miss, etc.), otherwise on a hit it returns the access latency.
 will also call the release_fn or arrange to have it be called when the 
 access latency is known.  release_fn will only be called if valid_fn == 
 (void *)0 or if it returns TRUE when passed obj and stamp (support for 
 speculative accesses. */

/* access a cache, perform a CMD operation on cache CP at address ADDR,
   places NBYTES of data at *P, returns latency of operation if initiated
   at NOW, *P is untouched if cache blocks are not allocated (!CP->BALLOC) */

int						/* latency of access in cycles */
cache_timing_access(tick_t now,			/* time of access */
		    cache_access_packet *c_packet)
{
  struct cache *cp = c_packet->cp;			/* Copy key arguments out of buffer */
  unsigned int cmd = c_packet->cmd;
  enum trans_cmd vorp = c_packet->vorp;
  struct cache_blk *blk;
  struct bus *bus;
  enum resource_type type;
  cache_access_packet *new_c_packet;
  void *next_mp;

  tick_t lat = 0, tlb_lat = 0;
  md_addr_t addr, baddr, vaddr = 0;
  md_addr_t tag, set, sb, bofs;
  int i;
  int mshr_index, target_index, size, subblock_miss = FALSE;


  vaddr = (vorp == Virtual) ? c_packet->addr : 0;
  assert(!((cp->trans == VIVT) && (vorp == Physical)));
  
  /* If we need to translate the address */
  if ((cp->trans != VIVT) && (vorp == Virtual))
    { 
      /* Translate the address */
      tlb_lat = cache_translate_address(now, c_packet, vaddr);

      /* If we had a TLB miss, discontinue the access for now */
      if (tlb_lat == TLB_MISS)
	{
	  return (tlb_lat);
	}
      if (c_packet->addr == BAD_ADDRESS) 
	{
	  cache_free_access_packet(c_packet);
	  return (BAD_ADDRESS);
	}

      /* The address is now a physical address */
      vorp = Physical;
    }
  c_packet->vorp = vorp;

  

  /* The address may have changed in cache_translate_address, so initialize 
     the local copy here */
  addr = c_packet->addr;

  baddr = CACHE_BADDR(cp, addr);
  bofs = CACHE_BLK(cp, addr);
  tag = CACHE_TAG(cp, addr);
  sb = CACHE_SB_TAG(cp, addr);
  set = (cp->trans == VIPT) ? CACHE_SET(cp, vaddr) : CACHE_SET(cp, addr);
  
  /* If we are simulating a perfect L2, return L2 hit latency */
  if (perfectl2 && (strcmp(cp->name,"L2") == 0)) {
    cache_free_access_packet(c_packet);
    return cp->hit_latency;
  }
  
  
  /* alignments were checked here, are now checked in check_address */
  /* check for a fast hit: access to same block */
  if (CACHE_TAGSET(cp, addr) == cp->last_tagset)
    {
      /* hit in the same block */
      blk = cp->last_blk;
      if (IS_BLOCK_SUBBLOCKED(blk) && !(CACHE_GET_SB_BIT(sb, blk->sb_valid)))
	subblock_miss = TRUE;
      else
	goto cache_fast_hit;
    }
  if (!cp->hsize)
    {
      /* low-associativity cache, linear search the way list */
      FIND_BLK_MATCH(cp, blk, set, tag, sb, subblock_miss, cache_hit)
    }
  else
    {
      /* high-associativity cache, access through the per-set hash tables */
      int hindex = CACHE_HASH(cp, tag);

      for (blk=cp->sets[set].hash[hindex];
	   blk;
	   blk=blk->hash_next)
	{
	  if (blk->tag == tag && (blk->status & CACHE_BLK_VALID)) {
	    if ((IS_BLOCK_SUBBLOCKED(blk)) && (!IS_SUBBLOCK_VALID(blk, sb))){
                subblock_miss = TRUE;
	    }
	    else {
	        goto cache_hit;
	    }
	  }
	}
    }

  /* cache block not found */
  cp->misses++;
  
    
  
  /* Check victim buffer for a hit */
  if (cp->victim_buffer) {
    struct cache_blk *repl;
    md_addr_t temp;
    FIND_ADDR_MATCH(blk, tag, set);
    if (blk) {
      victim_buf_hits++;
      /* hit */
      repl  = cp->sets[set].way_tail;
      update_way_list(&cp->sets[set], repl, Head);
      if (cp->hsize)
	unlink_htab_ent(cp, &cp->sets[set], repl);
      update_way_list(&vbuf, blk, Head);
      /* write back replaced block data */
      if (repl->status & CACHE_BLK_VALID){
	cp->replacements++;
	  
	if (repl->status & CACHE_BLK_DIRTY) {
	  do_writeback(now, cp, CACHE_MK_BADDR(cp, repl->tag, repl->set), vorp, repl);
	}
	temp = repl->tag;
	repl->tag = blk->tag;
	blk->tag = temp;
	repl->status = (CACHE_BLK_VALID | ((is_write(cmd))) ? CACHE_BLK_DIRTY : 0);
      }
      cache_free_access_packet(c_packet);
      if (cp->hsize)
	link_htab_ent(cp, &cp->sets[set], repl);
      return (cp->hit_latency+cp->victim_buffer);
    }
    else
      victim_buf_misses++;
  }
  /* is there a pending request for this blk? */
  FIND_MSHR_MATCH(cp, mshr_index, baddr, sb, subblock_miss, mshr_hit);
  
  /* Do we have a miss to this line for a diffrent address? If so trap 
     (presently works only for DL1) */
  if ((cache_diff_addr_trap && strcmp(cp->name, dcache_name)==0 && is_read(cmd))){
    for (mshr_index=0; mshr_index< MSHR_NREGULARS(cp);mshr_index++) {
      if ((MSHR_ADDR((cp), (mshr_index)) != baddr) && ((MSHR_ADDR2((cp), (mshr_index)) & cp->set_mask) == (c_packet->addr & cp->set_mask))) {
	cp->misses--;
	cache_free_access_packet(c_packet);
	cache_diffaddr_trap++;
	return TARGET_OCCUPIED;
      }
    }
  }
  /* mshr entry not found */
  /* allocate an entry */
  mshr_index = allocate_regular_mshr_register(cp);

  if (mshr_index == MSHRS_FULL)
    {
      cp->misses--;
      cp->mshr_full++;
      if (cache_mshrfull_trap && strcmp(cp->name, dcache_name)==0 && is_read(cmd)) {
	cache_free_access_packet(c_packet);
	return TARGET_OCCUPIED;
      }
      process_blocked_mshr(c_packet);
      /* Added by Raj - Need to translate the address again if the MSHRS are 
	 full if address translation was performed this cycle */
      if (vaddr != 0) {
	c_packet->addr = vaddr;
	c_packet->vorp = Virtual;
      }
      return MSHRS_FULL;
    }

  /* Increment miss counter */
  cp->mshr_misses++;

  assert(mshr_index < regular_mshrs);
  assert(MSHR_NREGULARS(cp) <= regular_mshrs);

  /* Find the bus that will be used to service the cache miss */
  /* We could have treated this as a generalized resource, like caches 
     and memory, but have hardcoded this in to improve performance */
  bus = bus_select(cp->num_resources, cp->resource_code, cp->resources);

  if (subblock_miss || (FETCH_SUBBLOCK(cp, blk, addr)))
    {
      cp->partial_misses++;
      cp->address_traffic += cp->subblock_vector_length;
      MSHR_SUBBLOCK_VECTOR(cp, mshr_index) = CREATE_SUBBLOCK_FETCH_VECTOR(cp, blk, sb);
      size = cp->sbsize * count_valid_bits(cp, MSHR_SUBBLOCK_VECTOR(cp, mshr_index));
    }
  else
    {
      size = cp->bsize;
    }

  cp->address_traffic += ADDRESS_SIZE;

  /* initialize the entry */
  MSHR_ADDR(cp, mshr_index) = baddr;
  MSHR_ADDR2(cp, mshr_index) = addr;
  MSHR_CMD(cp, mshr_index) = cmd;
  MSHR_BUS(cp, mshr_index) = bus;
  MSHR_SIZE(cp, mshr_index) = size;
  MSHR_INIT_TARGET(cp, mshr_index, /* target_index */0, now, c_packet);
  MSHR_NTARGETS(cp, mshr_index) = 1;

  if (cp->trans == VIPT)
    MSHR_VSET(cp, mshr_index) = set;

  /* Set the command for the access to the next level of the hierarchy */
  /* We don't just set it to Read in case it's a TLB access */
  /* This assumes write-allocate (we should implement different policies
     here eventually) */
  if (is_write(cmd))
    cmd = Mem_Read;

  /* Specify that this is a miss to a lower level of the hierarchy */
  cmd = (cmd & access_mask) | Miss_access;

  /* Time to resolve miss, generate request */
  /* In a VIPT cache, the TLB and tag lookups can be done in parallel, but in a PIPT
     cache they cannot, so must be added */
  lat += ((cp->trans == VIPT) ? MAX(cp->hit_latency, tlb_lat) : cp->hit_latency + tlb_lat);

  /* bus request, modeling contention */
  lat += bus_access(now, ADDRESS_SIZE, bus, Read_Request);

  next_mp = bus_follow(bus, &type);

  new_c_packet = cache_create_access_packet(next_mp, cmd, baddr, vorp, size,
					(void *) &cp->mshregisters[mshr_index],
					(RELEASE_FN_TYPE) schedule_response_handler, 
					(VALID_FN_TYPE) valid_mshr, 
					(int)MSHR_STAMP(cp, mshr_index));

  eventq_queue_callback2(now + lat, (void *) blk_access_fn, (int) new_c_packet, type);

  /* initiate prefetch */
#ifdef FLEXIBLE_SIM
  if (prefetch_dist) {
    md_addr_t prefetch_addr = baddr + cp->bsize;
    md_addr_t pre_set = set;
    for (i=0;i<prefetch_dist;i++, pre_set=CACHE_SET_INC(cp,pre_set)) {
      prefetch_addr = prefetch_addr+ i*cp->bsize;
      do_prefetch(now, cp, prefetch_addr, vorp, pre_set);
    }
  }
#else
  {
   md_addr_t prefetch_addr = baddr + cp->bsize;
   md_addr_t pre_set = set;
    for (i=0;i<prefetch_dist;i++, pre_set=CACHE_SET_INC(cp,pre_set)) {
      prefetch_addr = prefetch_addr+ i*cp->bsize;
      do_prefetch(now, cp, prefetch_addr, vorp, pre_set);
    }
  }
#endif
  
#if 0
  /* If there is a line predictor ..... */
  if (cp->line_predictor) {
    int i,j;
    md_addr_t temp = regs.regs_PC+sizeof(md_inst_t);
    fetch_inst_offset = CACHE_INST_OFFSET(cp,addr);
    cp->line_pred_lookups++;
    while (temp % (line_pred_width*sizeof(md_inst_t)) != 0)
      temp+=sizeof(md_inst_t);
    fetch_pred_PC = temp;
    temp = regs.regs_PC;
    for (i=0;i<cp->bsize/(line_pred_width*sizeof(md_inst_t));i++) {
      j=CACHE_INST_OFFSET(cp,temp);
      temp+=sizeof(md_inst_t);
      while (temp % (line_pred_width*sizeof(md_inst_t)) != 0)
	temp+=sizeof(md_inst_t);
      cp->line_pred_table[set][fetch_blk_no][j].next_addr = temp;
    }
    fetch_set = set;
  }
#endif
 
  return CACHE_MISS;

 mshr_hit:

  target_index = allocate_target(cp, mshr_index, c_packet->addr, c_packet->cmd);
  /* MSHR_NTARGETS is incremented properly in allocate_target() */

  if (target_index == TARGET_FULL)
    {
      cp->misses--;

      /* Fixme: would be a lot more efficient if we tied this event queue into freeing
	 of the specific mshr whose targets were full, instead of just any generic mshr
	 (forcing processing and requeueing until that one mshr is freed).  If full
	 targets happen a lot, should probably do that. */

      process_blocked_mshr(c_packet);
      /* Added by Raj - Need to translate the address again if the MSHRS
	 targets are full if address translation was performed this cycle */
      if (vaddr != 0) {
	c_packet->addr = vaddr;
	c_packet->vorp = Virtual;
      }
      return TARGET_FULL;
    }
  if (target_index == TARGET_OCCUPIED) {
    cp->misses--;
    cache_free_access_packet(c_packet);
    return TARGET_OCCUPIED;
  }

  assert(MSHR_NTARGETS(cp, mshr_index) <= mshr_targets);

  if (is_read(MSHR_CMD(cp, mshr_index)) && is_write(cmd))
    MSHR_CMD(cp, mshr_index) |= Mem_Write;

  MSHR_INIT_TARGET(cp, mshr_index, target_index, now + tlb_lat, c_packet);

  cp->mshr_hits++;
#if  0
  /* Write line predictor code here */
  /* If there is a line predictor ..... */
  if (cp->line_predictor) {
    int i,j;
    md_addr_t temp = regs.regs_PC+sizeof(md_inst_t);
    fetch_inst_offset = CACHE_INST_OFFSET(cp,addr); 
    cp->line_pred_lookups++;
    while (temp % (line_pred_width*sizeof(md_inst_t)) != 0)
      temp+=sizeof(md_inst_t);
    fetch_pred_PC = temp;
    temp = regs.regs_PC;
    for (i=0;i<cp->bsize/(line_pred_width*sizeof(md_inst_t));i++) {
      j=CACHE_INST_OFFSET(cp,temp);
      temp+=sizeof(md_inst_t);
      while (temp % (line_pred_width*sizeof(md_inst_t)) != 0)
	temp+=sizeof(md_inst_t);
      cp->line_pred_table[set][fetch_blk_no][j].next_addr = temp;
    }
    fetch_set = set;
  }
#endif
  return CACHE_MISS;

 cache_hit:
#if 0
  /* Write line predictor code here */
  /* If there is a line predictor ..... */
  if (cp->line_predictor) {
    fetch_inst_offset = CACHE_INST_OFFSET(cp,addr);
    fetch_blk_no = blk->blk_no;
    cp->line_pred_lookups++;
    if (cp->line_pred_table[set][fetch_blk_no][fetch_inst_offset].next_addr) {
      fetch_pred_PC = 
	cp->line_pred_table[set][fetch_blk_no][fetch_inst_offset].next_addr;
    }
    else {
      md_addr_t temp = regs.regs_PC + sizeof(md_inst_t);
      while (temp % (line_pred_width*sizeof(md_inst_t)) != 0)
	temp+=sizeof(md_inst_t);
      cp->line_pred_table[set][fetch_blk_no][fetch_inst_offset].next_addr = 
	fetch_pred_PC = temp;
      
    }
    fetch_set = set;
  }
  /* Write way predictor code here. If the way predictor is correct all is 
     well. else update the way predictor and incur 2 cycle miss penalty. 
     This is done by returning CACHE_MISS and scheduling a fetch unblock 
     event after 2 cycles. way_latency=2 for alpha.  Presently works only 
     with low associativity caches*/
  if (cp->way_pred_latency) {
    fetch_inst_offset = CACHE_INST_OFFSET(cp,addr);
    cp->way_pred_lookups++;
    if (cp->way_pred_table[fetch_set][fetch_blk_no][fetch_inst_offset] 
        == blk->blk_no) {
      /* Way predictor hit. nada*/
      cp->way_pred_hits++;
    }
    else {
      /* Update way table */
      cp->way_pred_misses++;
      cp->way_pred_table[fetch_set][fetch_blk_no][fetch_inst_offset] 
	= blk->blk_no;
      lat = ((cp->trans == VIPT) ? MAX(cp->hit_latency, tlb_lat) 
	                     : cp->hit_latency + tlb_lat);
      eventq_queue_callback(now+lat+cp->way_pred_latency, 
				(void *) fetch_resume_ifetch,
				(int) SAMPLE_STALL);
      cache_free_access_packet(c_packet);
      return CACHE_MISS;
    }
  }
#endif
  cp->hits++;

#ifdef BALLOC
  /* copy data out of cache block, if block exists */
  if (cp->balloc)
    {
      CACHE_BCOPY(cmd, blk, bofs, NULL, c_packet->nbytes);
    }
#endif

  /* update dirty status */
  if (is_write(cmd))
    blk->status |= CACHE_BLK_DIRTY;

  if (IS_BLOCK_SUBBLOCKED(blk))
    {
      if (is_write(cmd))
	CACHE_SET_SB_BIT(sb, blk->sb_dirty);
    } 

  /* prefetch if you will */
  if (cp->prefetch && !(blk->status & CACHE_BLK_TAG))
  {	
      do_prefetch(now, cp, baddr + cp->bsize, vorp, set);

      /* set the tag prefetch */
      blk->status |= CACHE_BLK_TAG;
  } 


  /* if LRU replacement and this is not the first element of list, reorder */
  if (blk->way_prev && cp->policy == LRU)
  {
    /* move this block to head of the way (MRU) list */
    update_way_list(&cp->sets[set], blk, Head);
  }

  /* tag is unchanged, so hash links (if they exist) are still valid */

  /* record the last block to hit */
  cp->last_tagset = CACHE_TAGSET(cp, addr);
  cp->last_blk = blk;

  cache_free_access_packet(c_packet);
  /* the release is done by the caller on a cache hit, so we don't have to
     do it here */
  /* Overlap tlb hit with data lookup if VIPT, otherwise they are serialized */
  /* (Unless cache is VIVT, in which case tlb_lat is always 0 */
  lat = ((cp->trans == VIPT) ? MAX(cp->hit_latency, tlb_lat) : cp->hit_latency + tlb_lat);
  return (int) MAX(lat, (blk->ready - now));

 cache_fast_hit: /* fast hit handler */
  //fetch_inst_offset =  CACHE_INST_OFFSET(cp,addr); 
#if 0
  /* Write line predictor code here */
  /* If there is a line predictor ..... */
  if (cp->line_predictor) {
    fetch_inst_offset = CACHE_INST_OFFSET(cp,addr);
    fetch_blk_no = blk->blk_no;
    cp->line_pred_lookups++;
    if (cp->line_pred_table[set][fetch_blk_no][fetch_inst_offset].next_addr) {
      fetch_pred_PC = 
	cp->line_pred_table[set][fetch_blk_no][fetch_inst_offset].next_addr;
    }
    else {
      md_addr_t temp = regs.regs_PC + sizeof(md_inst_t);
      while (temp % (line_pred_width*sizeof(md_inst_t)) != 0)
	temp+=sizeof(md_inst_t);
      cp->line_pred_table[set][fetch_blk_no][fetch_inst_offset].next_addr = 
	fetch_pred_PC = temp;
      
    }
    fetch_set = set;
  }
  /* Write way predictor code here. If the way predictor is correct all is 
     well. else update the way predictor and incur 2 cycle miss penalty. 
     This is done by returning CACHE_MISS and scheduling a fetch unblock 
     event after 2 cycles. way_latency=2 for alpha.  Presently works only 
     with low associativity caches*/
  if (cp->way_pred_latency) {
    fetch_inst_offset = CACHE_INST_OFFSET(cp,addr);
    cp->way_pred_lookups++;
    if (cp->way_pred_table[fetch_set][fetch_blk_no][fetch_inst_offset] 
	== blk->blk_no) {
      /* Way predictor hit. nada*/
      cp->way_pred_hits++;
    }
    else {
      /* Update way table */
      cp->way_pred_misses++;
      cp->way_pred_table[fetch_set][fetch_blk_no][fetch_inst_offset] 
	= blk->blk_no;
      lat = ((cp->trans == VIPT) ? MAX(cp->hit_latency, tlb_lat) 
	                     : cp->hit_latency + tlb_lat);
      eventq_queue_callback(now+lat+cp->way_pred_latency, 
				(void *) fetch_resume_ifetch,
				(int) SAMPLE_STALL);
      cache_free_access_packet(c_packet);
      return CACHE_MISS;
    }
  }
#endif
  cp->hits++;

#ifdef BALLOC
  /* copy data out of cache block, if block exists */
  if (cp->balloc)
    {
      CACHE_BCOPY(cmd, blk, bofs, p, nbytes);
    }
#endif

  /* update dirty status */
  if (is_write(cmd))
    blk->status |= CACHE_BLK_DIRTY;

  if (IS_BLOCK_SUBBLOCKED(blk))
    {
      if (is_write(cmd))
	CACHE_SET_SB_BIT(sb, blk->sb_dirty);
    } 

  /* prefetch if you will */
  if (cp->prefetch && !(blk->status & CACHE_BLK_TAG))
    {	
      do_prefetch(now, cp, baddr + cp->bsize, vorp, set);

      /* set the tag prefetch */
      blk->status |= CACHE_BLK_TAG;
    } 


  /* this block hit last, no change in the way list */
  /* tag is unchanged, so hash links (if they exist) are still valid */

  cache_free_access_packet(c_packet);
  
  /* return first cycle data is available to access */
  /* Overlap tlb hit with data lookup if VIPT, otherwise they are serialized */
  /* (Unless cache is VIVT, in which case tlb_lat is always 0 */
  lat = ((cp->trans == VIPT) ? MAX(cp->hit_latency, tlb_lat) 
	                     : cp->hit_latency + tlb_lat);
  return (int) MAX(lat, (blk->ready - now));
}

static void
response_handler(tick_t now,
		 struct mshregisters *mshrp, MSHR_STAMP_TYPE stamp)
{
  tick_t lat = 0;
  md_addr_t tag, set, sb, bofs;
  struct cache *cp;
  struct cache_blk *repl;
  struct bus *bus;
  enum trans_cmd vorp;
  int target_index;
  enum resource_type type;
  unsigned int cmd = MSHRP_CMD(mshrp);
  cp = MSHRP_CP(mshrp);
  bus = MSHRP_BUS(mshrp);
  

  tag = CACHE_TAG(cp, MSHRP_ADDR(mshrp));

  set = CACHE_SET(cp, MSHRP_ADDR(mshrp));
  bofs = CACHE_BLK(cp, MSHRP_ADDR(mshrp));
  sb = CACHE_SB_TAG(cp, MSHRP_ADDR(mshrp));

  vorp = (cp->trans == VIVT) ? Virtual : Physical;
  if (cp->trans == VIPT) 
    {
      set = MSHRP_VSET(mshrp);
    }

  /* Time to process returned data (doesn't count bus time) */
  //lat = cp->hit_latency;
  /* time, response sent through the bus, critical word first */
  lat += bus_access(now, MSHRP_SIZE(mshrp), bus, Read_Response);

#ifdef L2BYPASS
  (void) bus_follow(bus, &type);
  if (type == Memory) 
    lat = 1;
#endif

  cp->read_traffic += MSHRP_SIZE(mshrp);
  cp->address_traffic += ADDRESS_SIZE;

  assert(MSHRP_STAMP(mshrp) == stamp);
  assert(mshrp->addr != UNUSED_MSHREGISTER);

  if ((MSHRP_SUBBLOCK_VECTOR(mshrp)) && (repl = find_blk_match_no_jump(cp, set, tag)))
    {
      repl->sb_valid |= MSHRP_SUBBLOCK_VECTOR(mshrp);
      repl->ready = now + cp->hit_latency + (bus->arbitration + MSHRP_SIZE(mshrp)/bus->width) * bus->clock_differential;
      if (is_write(MSHRP_CMD(mshrp)))
	{
	  repl->status |= CACHE_BLK_DIRTY;
	  CACHE_SET_SB_BIT(sb, repl->sb_dirty);
	}
    }
  else
    {	
      /* select block to replace */
      switch (cp->policy) {
      case LRF:
      case LRU:
      case FIFO:
	repl = cp->sets[set].way_tail;
#if 0
	if (cp->line_predictor) {
	  fetch_blk_no = repl->blk_no;
	}
#endif
	/* update way list to Head if this is not a prefetch, else 
	   update to tail */
	if (cmd & Mem_Prefetch)
	  update_way_list(&cp->sets[set], repl, Tail);
	else
	  update_way_list(&cp->sets[set], repl, Head);
	break;
      case Random:
	{
#if defined(__CYGWIN32__) || defined(hpux) || defined(__hpux) || defined(__svr4__)
	  int bindex = rand() & (cp->assoc - 1);
#else
	  int bindex = random() & (cp->assoc - 1);
#endif
	  repl = CACHE_BINDEX(cp, cp->sets[set].blks, bindex);
#if 0
	  if (cp->line_predictor) {
	    fetch_blk_no = repl->blk_no;
	  }
#endif
	}
	break;
      default:
	panic("bogus replacement policy");
      }
      
      /* remove this block from the hash bucket chain, if hash exists */
      if (cp->hsize)
	unlink_htab_ent(cp, &cp->sets[set], repl);
      /* Update victim buffer */
      if (cp->victim_buffer) {
	vbuf.way_tail->tag = repl->tag;
	vbuf.way_tail->set = repl->set;
	update_way_list(&vbuf, vbuf.way_tail, Head);
      }      
      /* write back replaced block data */
      if (repl->status & CACHE_BLK_VALID)
	{
	  cp->replacements++;
	  
	  if (repl->status & CACHE_BLK_DIRTY)
	    {
	      do_writeback(now, cp, CACHE_MK_BADDR(cp, repl->tag, repl->set), vorp, repl);
	    }
	}

      /* update block tags */
      repl->tag = tag;
      /* Save set from original address to use in case of writebacks (needed 
	 because of VIPT caches */
      repl->set = CACHE_SET(cp, MSHRP_ADDR(mshrp));
      repl->status = CACHE_BLK_VALID | ((is_write(MSHRP_CMD(mshrp))) ? CACHE_BLK_DIRTY : 0);
      repl->ready = now + cp->hit_latency + (bus->arbitration + MSHRP_SIZE(mshrp)/bus->width) * bus->clock_differential;
      repl->sb_valid = repl->sb_dirty = 0;

      if (MSHRP_SUBBLOCK_VECTOR(mshrp))
	{
	  repl->sb_valid = MSHRP_SUBBLOCK_VECTOR(mshrp);
	  if (is_write(MSHRP_CMD(mshrp)))
	    CACHE_SET_SB_BIT(sb, repl->sb_dirty);
	}

      /* Blow away last tagset so we don't fake a hit */
      cp->last_tagset = 0xffffffff;
      
      /* link this entry back into the hash table */
      if (cp->hsize)
	link_htab_ent(cp, &cp->sets[set], repl);
    }

  /* send as many responses as there is recipient */
  for (target_index = 0; target_index < MSHRP_NTARGETS(mshrp); target_index++)
    {
      if (!MSHRP_RELEASE_FN(mshrp, target_index))
	/* no release function to call */
	continue;
      if (MSHRP_VALID_FN(mshrp, target_index))
	if (!MSHRP_VALID_TARGET(mshrp, target_index))
	  /* squashed target, don't release */
	  continue;

#ifdef BALLOC
      /* copy data out of cache block */
      if (cp->balloc)
	{
	  CACHE_BCOPY(MSHRP_CMD(mshrp), repl, bofs, MSHRP_DATA(mshrp, target_index), cp->bsize);
	}
#endif

      MSHRP_RELEASE(mshrp, target_index, now + lat);
    }

  /* Free all the packets allocated for the various misses */
  for (target_index = 0; target_index < MSHRP_NTARGETS(mshrp); target_index++)
    {
      cache_free_access_packet(MSHRP_PKT(mshrp, target_index));
    }

  if (!MSHRP_PREFETCH(mshrp))
    {
      /* deallocate regular mshr register */
      MSHR_NREGULARS(cp)--;
      MSHRP_ADDR(mshrp) = UNUSED_MSHREGISTER;
    }
  else
    {
      /* deallocate prefetch mshr register */
      md_addr_t addr = MSHRP_ADDR(mshrp);

      /* deallocate mshr entry */
      MSHR_NPREFETCH(cp)--;
      MSHRP_ADDR(mshrp) = UNUSED_MSHREGISTER;

      if (MSHRP_NTARGETS(mshrp) > 1)
	{
	  /* at least one access attempted to prefetch block, prefetch next
	     block */
	  repl->status |= CACHE_BLK_TAG;
	  do_prefetch(now, cp, addr + cp->bsize, vorp, set);
	}
    }

  /* If we had a pending event blocked because of full MSHRS, issue one event */
  /* Fixme: If we're doing prefetching, may want to initiate this event before sending a
     prefetch (giving pending events higher priority */
  if (cp->mshr_full_head)
      process_mshr_full_event(now, cp);
}


void
schedule_response_handler(tick_t when,
			  struct mshregisters *mshrp, 
			  MSHR_STAMP_TYPE stamp)
{
  eventq_queue_callback2(when, (void *)response_handler,
			 (int)mshrp, (int)stamp);
}

/* allocate one regular (non-prefetch) mshr register.  salvage one if
   necessary */
static int
allocate_regular_mshr_register(struct cache *cp)
{
  int which_register;

  if (MSHR_NREGULARS(cp) < regular_mshrs)
    {
      /* there is at least one free register, linear search */
      for (which_register = 0;
	   which_register < regular_mshrs;
	   which_register++)
	if (MSHR_ADDR(cp, which_register) == UNUSED_MSHREGISTER)
	  break;

      MSHR_NREGULARS(cp)++;

      /* Set the highest number MSHR used, if needed, to reduce searching
	 for mshr match */
      if (which_register > MSHR_MAXREGULARS(cp))
	{
	  MSHR_MAXREGULARS(cp) = which_register;
	}

      return which_register;
    }

  /* Removed reclaimation code, since we don't need to reclaim squashed registers 
     as far as I can tell */

  /* Couldn't find a free MSHR, return this negative value that will hold the mem 
     op in the pipeline */
  return MSHRS_FULL;
}

/* allocate a target, salvage one if necessary */
static int
allocate_target(struct cache *cp, 
		int which_register,
		md_addr_t addr,
		unsigned int cmd)
{
  int which_target;
  int i;
  if (MSHR_NTARGETS(cp, which_register) < mshr_targets) {
      /* Check if quadwords match. Return TARGET_OCCUPIED on a match */
    if (cache_target_trap && strcmp(cp->name, dcache_name)==0 && is_read(cmd)) 
      for (i=0;i<MSHR_NTARGETS(cp, which_register);i++) {
	if ((MSHR_RADDR(cp, which_register, i)) >> 3 == (addr >> 3)) {
	  cache_quadword_trap++;
	  return TARGET_OCCUPIED;
	}
	//return TARGET_FULL;
      }
      /* free target available */
      which_target = MSHR_NTARGETS(cp, which_register)++;
      return which_target;
    }

  /* Was code here to reclaim targets if release function was null, or
     if valid function was non-null but returned false (i.e. mis-spec).
     Now that we can block on full targets, no reclaimation no longer
     needed. */

  /* No more targets for this MSHR, stall the mem op in the pipeline */
  return TARGET_FULL;
}

long
valid_mshr(struct mshregisters *mshrp, 
	   MSHR_STAMP_TYPE stamp)
{
  return (MSHRP_STAMP(mshrp) == stamp);
}


/* flush the entire cache, returns latency of the operation */

unsigned int					/* latency of the flush operation */
cache_timing_flush(struct cache *cp,		/* cache instance to flush */
		   tick_t now)			/* time of cache flush */
{
  int i, lat = cp->hit_latency; /* min latency to probe cache */
  struct cache_blk *blk;
  enum trans_cmd vorp = (cp->trans == VIVT) ? Virtual : Physical;

  /* blow away the last block to hit */
  cp->last_tagset = 0;
  cp->last_blk = NULL;

  /* no way list updates required because all blocks are being invalidated */
  for (i=0; i<cp->nsets; i++)
    {
      for (blk=cp->sets[i].way_head; blk; blk=blk->way_next)
	{
	  if (blk->status & CACHE_BLK_VALID)
	    {
	      cp->invalidations++;
	      blk->status &= ~CACHE_BLK_VALID;

	      if (blk->status & CACHE_BLK_DIRTY)
		{
		  /* This look could be made more efficient by pulling out the 
		     write bus and memory level lookup from do_writeback and putting
		     them before the loop entry ... but I don't expect to be using
		     cache_flush all that often. */
		    do_writeback(now + lat, cp, CACHE_MK_BADDR(cp, blk->tag, blk->set), vorp, blk);
		    assert((cp->trans == VIPT) ? 1 : (blk->set == i));
		}
	    }
	}
    }

  /* return latency of the flush operation */
  return lat;
}

/* flush the block containing ADDR from the cache CP, returns the latency of
   the block flush operation */
/* Does not currently support subblock flushing */

unsigned int					/* latency of flush operation */
cache_timing_flush_addr(struct cache *cp,	/* cache instance to flush */
			md_addr_t addr,	/* address of block to flush */
			tick_t now) /* time of cache flush */
{
  md_addr_t tag = CACHE_TAG(cp, addr);
  md_addr_t set = CACHE_SET(cp, addr);
  struct cache_blk *blk;
  int lat = cp->hit_latency; /* min latency to probe cache */
  enum trans_cmd vorp = ((cp->trans == VIVT) ? Virtual : Physical);

  if (cp->hsize)
    {
      /* higly-associativity cache, access through the per-set hash tables */
      int hindex = CACHE_HASH(cp, tag);

      for (blk=cp->sets[set].hash[hindex];
	   blk;
	   blk=blk->hash_next)
	{
	  if (blk->tag == tag && (blk->status & CACHE_BLK_VALID))
	    break;
	}
    }
  else
    {
      /* low-associativity cache, linear search the way list */
      for (blk=cp->sets[set].way_head;
	   blk;
	   blk=blk->way_next)
	{
	  if (blk->tag == tag && (blk->status & CACHE_BLK_VALID))
	    break;
	}
    }

  if (blk)
    {
      cp->invalidations++;
      blk->status &= ~CACHE_BLK_VALID;

      /* blow away the last block to hit */
      cp->last_tagset = 0;
      cp->last_blk = NULL;

      if (blk->status & CACHE_BLK_DIRTY)
	{
	  do_writeback(now + lat, cp, CACHE_MK_BADDR(cp, blk->tag, blk->set), vorp, blk);
	  assert((cp->trans == VIPT) ? 1 : (blk->set == set));
	}
      /* move this block to tail of the way (LRU) list */
      update_way_list(&cp->sets[set], blk, Tail);
    }

  /* return latency of the operation */
  return lat;
}

