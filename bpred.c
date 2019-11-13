/*
 * bpred.c - branch predictor routines
 *
 * This file is written by Fuxin Zhang, based on bpred.c of SimpleScalar
 * tool suite.
 *
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
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
 * INTERNET: dburger@cs.wisc.edu
 * US Mail:  1210 W. Dayton Street, Madison, WI 53706
 *
 */

/*
 * Redistributions of any form whatsoever must retain and/or include the 
 * following acknowledgment, notices and disclaimer:
 *
 * This product includes software developed by Carnegie Mellon University. 
 *
 * Copyright (c) 2003 by Babak Falsafi and James Hoe for the
 * SimFlex Project, Computer Architecture Lab at Carnegie Mellon,
 * Carnegie Mellon University
 *
 * This source file includes SMARTSim extensions originally written by
 * Thomas Wenisch and Roland Wunderlich of the SimFlex Project. 
 *
 * For more information, see the SimFlex project website at:
 *   http://www.ece.cmu.edu/~simflex
 *
 * You may not use the name "Carnegie Mellon University" or derivations 
 * thereof to endorse or promote products derived from this software.
 *
 * If you modify the software you must place a notice on or within any 
 * modified version provided or made available to any third party stating 
 * that you have modified the software.  The notice shall include at least 
 * your name, address, phone number, email address and the date and purpose 
 * of the modification.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" WITHOUT ANY WARRANTY OF ANY KIND, EITHER 
 * EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO ANY WARRANTY 
 * THAT THE SOFTWARE WILL CONFORM TO SPECIFICATIONS OR BE ERROR-FREE AND ANY 
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, 
 * TITLE, OR NON-INFRINGEMENT.  IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY 
 * BE LIABLE FOR ANY DAMAGES, INCLUDING BUT NOT LIMITED TO DIRECT, INDIRECT, 
 * SPECIAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF, RESULTING FROM, OR IN 
 * ANY WAY CONNECTED WITH THIS SOFTWARE (WHETHER OR NOT BASED UPON WARRANTY, 
 * CONTRACT, TORT OR OTHERWISE).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "cmu-config.h"
#include "host.h"
#include "misc.h"
#include "mips.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "cache.h"
#include "bpred.h"
#include "godson2_cpu.h"
#include "sim.h"

/* turn this on to enable the SimpleScalar 2.0 RAS bug */
/* #define RAS_BUG_COMPATIBLE */
static struct bpred_t bpred;
static struct bpred_t *pred = &bpred;

/* branch predictor type {nottaken|taken|perfect|bimod|2lev} */
char *pred_type;

/* bimodal predictor config (<table_size>) */
int bimod_nelt = 1;
int bimod_config[1] =
  { /* bimod tbl size */2048 };

/* 2-level predictor config (<l1size> <l2size> <hist_size> <xor>) */
int twolev_nelt = 4;
int twolev_config[4] = 
  { /* l1size */1, /* l2size */4096, /* hist */9, /* xor */FALSE};

/* combining predictor config (<meta_table_size> */
int comb_nelt = 1;
int comb_config[1] = 
  { /* meta_table_size */1024 };

/* 21264 predictor config (<l1size> <l2size> <l_hist_size>) 
                          (<gsize> <g_hist_size>)
			  (<csize> <c_hist_size>) */
int pred_21264_nelt = 7;
int pred_21264_config[7] = 
  { /* l1size */1024, /* l2size */1024, /* lhist */8,
    /* gsize */ 4096, /* ghist */ 4,
    /* csize */ 4096, /* chist */ 4 };
/* return address stack (RAS) size */
int ras_size = 4;

/* BTB predictor config (<num_sets> <associativity>) */
int btb_nelt = 2;
int btb_config[2] = 
  { /* nsets */128, /* assoc */1 };

/* perfect prediction enabled */
int pred_perfect = FALSE;

/* 21264 predictor global and choice indices */

int gindex=0;
int cindex=0;
int specgindex=0;
int speccindex=0;
//static char result = 0;

/* Should we update the branch predictor speculatively */
int bpred_spec_update;

/* create a branch predictor */
struct bpred_t *			/* branch predictory instance */
bpred_create(enum bpred_class class,	/* type of predictor to create */
	     unsigned int bimod_size,	/* bimod table size */
	     unsigned int l1size,	/* 2lev l1 table size */
	     unsigned int l2size,	/* 2lev l2 table size */
	     unsigned int meta_size,	/* meta table size */
	     unsigned int shift_width,	/* history register width */
	     unsigned int xor,  	/* history xor address flag */
	     unsigned int btb_sets,	/* number of sets in BTB */
	     unsigned int btb_assoc,	/* BTB associativity */
	     unsigned int retstack_size,/* num entries in ret-addr stack */
             unsigned int gsize,        /* 21264 global size */
             unsigned int ghist,        /* 21264 hist size */
             unsigned int csize,        /* 21264 choice size */
             unsigned int chist)        /* 21264 choice hist */

{
  struct bpred_t *pred;

  if (!(pred = calloc(1, sizeof(struct bpred_t))))
    fatal("out of virtual memory");

  pred->class = class;

  switch (class) {
  case BPredComb:
    /* bimodal component */
    pred->dirpred.bimod =
      bpred_dir_create(BPred2bit, bimod_size, 0, 0, 0);

    /* 2-level component */
    pred->dirpred.twolev =
      bpred_dir_create(BPred2Level, l1size, l2size, shift_width, xor);

    /* metapredictor component */
    pred->dirpred.meta =
      bpred_dir_create(BPred2bit, meta_size, 0, 0, 0);

    break;

  case BPred2Level:
    pred->dirpred.twolev =
      bpred_dir_create(class, l1size, l2size, shift_width, xor);

    break;

  case BPred2bit:
    pred->dirpred.bimod =
      bpred_dir_create(class, bimod_size, 0, 0, 0);

  case BPredTaken:
  case BPredNotTaken:
    /* no other state */
    break;

  default:
    panic("bogus predictor class");
  }

  /* allocate ret-addr stack */
  switch (class) {
  case BPredComb:
  case BPred2Level:
  case BPred2bit:
    {
      int i;

      /* allocate BTB */
      if (!btb_sets || (btb_sets & (btb_sets-1)) != 0)
	fatal("number of BTB sets must be non-zero and a power of two");
      if (!btb_assoc || (btb_assoc & (btb_assoc-1)) != 0)
	fatal("BTB associativity must be non-zero and a power of two");

      if (!(pred->btb.btb_data = calloc(btb_sets * btb_assoc,
					sizeof(struct bpred_btb_ent_t))))
	fatal("cannot allocate BTB");

      pred->btb.sets = btb_sets;
      pred->btb.assoc = btb_assoc;

      if (pred->btb.assoc > 1)
	for (i=0; i < (pred->btb.assoc*pred->btb.sets); i++)
	  {
	    if (i % pred->btb.assoc != pred->btb.assoc - 1)
	      pred->btb.btb_data[i].next = &pred->btb.btb_data[i+1];
	    else
	      pred->btb.btb_data[i].next = NULL;

	    /* godson store partial address here, initialized to an impossible address tag */
	    pred->btb.btb_data[i].addr = 0xffffffff;

	    if (i % pred->btb.assoc != pred->btb.assoc - 1)
	      pred->btb.btb_data[i+1].prev = &pred->btb.btb_data[i];
	  }

	  /* allocate retstack */
	  if ((retstack_size & (retstack_size-1)) != 0)
		fatal("Return-address-stack size must be zero or a power of two");

	  pred->retstack.size = retstack_size;
	  if (retstack_size)
		if (!(pred->retstack.stack = calloc(retstack_size,
				sizeof(struct bpred_btb_ent_t))))
		  fatal("cannot allocate return-address-stack");
	  pred->retstack.tos = retstack_size - 1;

	  break;
    }

  case BPredTaken:
  case BPredNotTaken:
    /* no other state */
    break;

  default:
    panic("bogus predictor class");
  }

  return pred;
}

/* create a branch direction predictor */
struct bpred_dir_t *		/* branch direction predictor instance */
bpred_dir_create (
  enum bpred_class class,	/* type of predictor to create */
  unsigned int l1size,	 	/* level-1 table size */
  unsigned int l2size,	 	/* level-2 table size (if relevant) */
  unsigned int shift_width,	/* history register width */
  unsigned int xor)	    	/* history xor address flag */
{
  struct bpred_dir_t *pred_dir;
  unsigned int cnt;
  int flipflop;

  if (!(pred_dir = calloc(1, sizeof(struct bpred_dir_t))))
    fatal("out of virtual memory");

  pred_dir->class = class;

  cnt = -1;
  switch (class) {
  case BPred2Level:
    {
      if (!l1size || (l1size & (l1size-1)) != 0 || l1size > MAX_L1_SIZE)
	fatal("level-1 size, `%d', must be non-zero and a power of two and less than MAX_L1_SIZE",
	      l1size);
      pred_dir->config.two.l1size = l1size;

      if (!l2size || (l2size & (l2size-1)) != 0)
	fatal("level-2 size, `%d', must be non-zero and a power of two",
	      l2size);
      pred_dir->config.two.l2size = l2size;

      if (!shift_width || shift_width > 30)
	fatal("shift register width, `%d', must be non-zero and positive",
	      shift_width);
      pred_dir->config.two.shift_width = shift_width;
      pred_dir->config.two.shift_mask = (1 << shift_width) - 1;

      pred_dir->config.two.xor = xor;
      pred_dir->config.two.shiftregs = calloc(l1size, sizeof(int));
      if (!pred_dir->config.two.shiftregs)
	fatal("cannot allocate shift register table");

      pred_dir->config.two.l2table = calloc(l2size, sizeof(unsigned char));
      if (!pred_dir->config.two.l2table)
	fatal("cannot allocate second level table");

#if 0
      /* initialize counters to weakly this-or-that */
      flipflop = 1;
      for (cnt = 0; cnt < l2size; cnt++)
	{
	  pred_dir->config.two.l2table[cnt] = flipflop;
	  flipflop = 3 - flipflop;
	}
#endif

      break;
    }

  case BPred2bit:
    if (!l1size || (l1size & (l1size-1)) != 0)
      fatal("2bit table size, `%d', must be non-zero and a power of two",
	    l1size);
    pred_dir->config.bimod.size = l1size;
    if (!(pred_dir->config.bimod.table =
	  calloc(l1size, sizeof(unsigned char))))
      fatal("cannot allocate 2bit storage");
    /* initialize counters to weakly this-or-that */
    flipflop = 1;
    for (cnt = 0; cnt < l1size; cnt++)
      {
	pred_dir->config.bimod.table[cnt] = flipflop;
	flipflop = 3 - flipflop;
      }

    break;

  case BPredTaken:
  case BPredNotTaken:
    /* no other state */
    break;

  default:
    panic("bogus branch direction predictor class");
  }

  return pred_dir;
}

/* print branch direction predictor configuration */
void
bpred_dir_config(
  struct bpred_dir_t *pred_dir,	/* branch direction predictor instance */
  char name[],			/* predictor name */
  FILE *stream)			/* output stream */
{
  switch (pred_dir->class) {
  case BPred2Level:
    fprintf(stream,
      "pred_dir: %s: 2-lvl: %d l1-sz, %d bits/ent, %s xor, %d l2-sz, direct-mapped\n",
      name, pred_dir->config.two.l1size, pred_dir->config.two.shift_width,
      pred_dir->config.two.xor ? "" : "no", pred_dir->config.two.l2size);
    break;

  case BPred2bit:
    fprintf(stream, "pred_dir: %s: 2-bit: %d entries, direct-mapped\n",
      name, pred_dir->config.bimod.size);
    break;

  case BPredTaken:
    fprintf(stream, "pred_dir: %s: predict taken\n", name);
    break;

  case BPredNotTaken:
    fprintf(stream, "pred_dir: %s: predict not taken\n", name);
    break;

  default:
    panic("bogus branch direction predictor class");
  }
}

/* print branch predictor configuration */
void
bpred_config(struct bpred_t *pred,	/* branch predictor instance */
	     FILE *stream)		/* output stream */
{
  switch (pred->class) {
  case BPredComb:
    bpred_dir_config (pred->dirpred.bimod, "bimod", stream);
    bpred_dir_config (pred->dirpred.twolev, "2lev", stream);
    bpred_dir_config (pred->dirpred.meta, "meta", stream);
    fprintf(stream, "btb: %d sets x %d associativity",
	    pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPred2Level:
    bpred_dir_config (pred->dirpred.twolev, "2lev", stream);
    fprintf(stream, "btb: %d sets x %d associativity",
	    pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPred2bit:
    bpred_dir_config (pred->dirpred.bimod, "bimod", stream);
    fprintf(stream, "btb: %d sets x %d associativity",
	    pred->btb.sets, pred->btb.assoc);
    fprintf(stream, "ret_stack: %d entries", pred->retstack.size);
    break;

  case BPredTaken:
    bpred_dir_config (pred->dirpred.bimod, "taken", stream);
    break;
  case BPredNotTaken:
    bpred_dir_config (pred->dirpred.bimod, "nottaken", stream);
    break;

  default:
    panic("bogus branch predictor class");
  }
}

/* print predictor stats */
void
bpred_stats(struct bpred_t *pred,	/* branch predictor instance */
	    FILE *stream)		/* output stream */
{
  fprintf(stream, "pred: addr-prediction rate = %f\n",
	  (double)pred->addr_hits/(double)(pred->addr_hits+pred->misses));
  fprintf(stream, "pred: dir-prediction rate = %f\n",
	  (double)pred->dir_hits/(double)(pred->dir_hits+pred->misses));
}

/* register branch predictor stats */
void
bpred_reg_stats(struct bpred_t *pred,	/* branch predictor instance */
		struct stat_sdb_t *sdb)	/* stats database */
{
  char buf[512], buf1[512], *name;

  /* get a name for this predictor */
  switch (pred->class)
    {
    case BPredComb:
      name = "bpred_comb";
      break;
    case BPred2Level:
      name = "bpred_2lev";
      break;
    case BPred2bit:
      name = "bpred_bimod";
      break;
    case BPredTaken:
      name = "bpred_taken";
      break;
    case BPredNotTaken:
      name = "bpred_nottaken";
      break;
    default:
      panic("bogus branch predictor class");
    }

  sprintf(buf, "%s.lookups", name);
  stat_reg_counter(sdb, buf, "total number of bpred lookups",
		   &pred->lookups, 0, NULL);
  sprintf(buf, "%s.updates", name);
  sprintf(buf1, "%s.dir_hits + %s.misses", name, name);
  stat_reg_formula(sdb, buf, "total number of updates", buf1, "%12.0f");
  sprintf(buf, "%s.addr_hits", name);
  stat_reg_counter(sdb, buf, "total number of address-predicted hits",
		   &pred->addr_hits, 0, NULL);
  sprintf(buf, "%s.dir_hits", name);
  stat_reg_counter(sdb, buf,
		   "total number of direction-predicted hits "
		   "(includes addr-hits)",
		   &pred->dir_hits, 0, NULL);
  if (pred->class == BPredComb)
    {
      sprintf(buf, "%s.used_bimod", name);
      stat_reg_counter(sdb, buf,
		       "total number of bimodal predictions used",
		       &pred->used_bimod, 0, NULL);
      sprintf(buf, "%s.used_2lev", name);
      stat_reg_counter(sdb, buf,
		       "total number of 2-level predictions used",
		       &pred->used_2lev, 0, NULL);
    }
  sprintf(buf, "%s.misses", name);
  stat_reg_counter(sdb, buf, "total number of misses", &pred->misses, 0, NULL);
  sprintf(buf, "%s.jr_hits", name);
  stat_reg_counter(sdb, buf,
		   "total number of address-predicted hits for JR's",
		   &pred->jr_hits, 0, NULL);
  sprintf(buf, "%s.jr_seen", name);
  stat_reg_counter(sdb, buf,
		   "total number of JR's seen",
		   &pred->jr_seen, 0, NULL);
  sprintf(buf, "%s.jr_non_ras_hits.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of address-predicted hits for non-RAS JR's",
		   &pred->jr_non_ras_hits, 0, NULL);
  sprintf(buf, "%s.jr_non_ras_seen.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of non-RAS JR's seen",
		   &pred->jr_non_ras_seen, 0, NULL);
  sprintf(buf, "%s.bpred_addr_rate", name);
  sprintf(buf1, "%s.addr_hits / %s.updates", name, name);
  stat_reg_formula(sdb, buf,
		   "branch address-prediction rate (i.e., addr-hits/updates)",
		   buf1, "%9.4f");
  sprintf(buf, "%s.bpred_dir_rate", name);
  sprintf(buf1, "%s.dir_hits / %s.updates", name, name);
  stat_reg_formula(sdb, buf,
		  "branch direction-prediction rate (i.e., all-hits/updates)",
		  buf1, "%9.4f");
  sprintf(buf, "%s.bpred_jr_rate", name);
  sprintf(buf1, "%s.jr_hits / %s.jr_seen", name, name);
  stat_reg_formula(sdb, buf,
		  "JR address-prediction rate (i.e., JR addr-hits/JRs seen)",
		  buf1, "%9.4f");
  sprintf(buf, "%s.bpred_jr_non_ras_rate.PP", name);
  sprintf(buf1, "%s.jr_non_ras_hits.PP / %s.jr_non_ras_seen.PP", name, name);
  stat_reg_formula(sdb, buf,
		   "non-RAS JR addr-pred rate (ie, non-RAS JR hits/JRs seen)",
		   buf1, "%9.4f");
  sprintf(buf, "%s.retstack_pushes", name);
  stat_reg_counter(sdb, buf,
		   "total number of address pushed onto ret-addr stack",
		   &pred->retstack_pushes, 0, NULL);
  sprintf(buf, "%s.retstack_pops", name);
  stat_reg_counter(sdb, buf,
		   "total number of address popped off of ret-addr stack",
		   &pred->retstack_pops, 0, NULL);
  sprintf(buf, "%s.used_ras.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of RAS predictions used",
		   &pred->used_ras, 0, NULL);
  sprintf(buf, "%s.ras_hits.PP", name);
  stat_reg_counter(sdb, buf,
		   "total number of RAS hits",
		   &pred->ras_hits, 0, NULL);
  sprintf(buf, "%s.ras_rate.PP", name);
  sprintf(buf1, "%s.ras_hits.PP / %s.used_ras.PP", name, name);
  stat_reg_formula(sdb, buf,
		   "RAS prediction rate (i.e., RAS hits/used RAS)",
		   buf1, "%9.4f");
}

void
bpred_after_priming(struct bpred_t *bpred)
{
  if (bpred == NULL)
    return;

  bpred->lookups = 0;
  bpred->addr_hits = 0;
  bpred->dir_hits = 0;
  bpred->used_ras = 0;
  bpred->used_bimod = 0;
  bpred->used_2lev = 0;
  bpred->jr_hits = 0;
  bpred->jr_seen = 0;
  bpred->misses = 0;
  bpred->retstack_pops = 0;
  bpred->retstack_pushes = 0;
  bpred->ras_hits = 0;
}

#define BIMOD_HASH(PRED, ADDR)						\
  ((((ADDR) >> 19) ^ ((ADDR) >> MD_BR_SHIFT)) & ((PRED)->config.bimod.size-1))
    /* was: ((baddr >> 16) ^ baddr) & (pred->dirpred.bimod.size-1) */

/* predicts a branch direction */
char *						/* pointer to counter */
bpred_dir_lookup(struct bpred_dir_t *pred_dir,	/* branch dir predictor inst */
		 md_addr_t baddr)		/* branch address */
{
  unsigned char *p = NULL;

  /* Except for jumps, get a pointer to direction-prediction bits */
  switch (pred_dir->class) {
    case BPred2Level:
      {
	int l1index, l2index;

        /* traverse 2-level tables */
        //l1index = (baddr >> MD_BR_SHIFT) & (pred_dir->config.two.l1size - 1);
        //l2index = pred_dir->config.two.shiftregs[l1index];
#if 0
        if (pred_dir->config.two.xor)
	  {
#if 1
	    /* this L2 index computation is more "compatible" to McFarling's
	       verison of it, i.e., if the PC xor address component is only
	       part of the index, take the lower order address bits for the
	       other part of the index, rather than the higher order ones */
	    l2index = (((l2index ^ (baddr >> MD_BR_SHIFT))
			& ((1 << pred_dir->config.two.shift_width) - 1))
		       | ((baddr >> MD_BR_SHIFT)
			  << pred_dir->config.two.shift_width));
#else
	    l2index = l2index ^ (baddr >> MD_BR_SHIFT);
#endif
	  }
	else
	  {
	    l2index =
	      l2index
		| ((baddr >> MD_BR_SHIFT) << pred_dir->config.two.shift_width);
	  }

#else /* godson2 */
	// rindex  = ((ghr<<(Npht_index_bits-Nghrxor))^(pc/Nblocksize_i))&PHT_index_mask;
	/* godson2 fetch 8 counter per access */
#if 0
	printf("ghr=%x",l2index);
#endif
        l2index = pred_dir->config.two.shiftregs[0];
	//l2index = 0;
	l2index = (( baddr >> 5 /* FIXME: icache->bsize */) ^ l2index) & pred_dir->config.two.shift_mask;
	//printf(" index1=%x",l2index);
	l2index = (l2index << 3) + ((baddr & 0x1f) >> 2); /* offset */
	//printf(" index2=%x",l2index);
#endif
        l2index = l2index & (pred_dir->config.two.l2size - 1);
#if 0
	printf(" index=%x,val=%x\n",l2index,pred_dir->config.two.l2table[l2index]);
#endif

        /* get a pointer to prediction state information */
        p = &pred_dir->config.two.l2table[l2index];
      }
      break;
    case BPred2bit:
      p = &pred_dir->config.bimod.table[BIMOD_HASH(pred_dir, baddr)];
      break;
    case BPredTaken:
    case BPredNotTaken:
      break;
    default:
      panic("bogus branch direction predictor class");
    }

  return (char *)p;
}

/* probe a predictor for a next fetch address, the predictor is probed
   with current start fetch pc CURPC. the BTB result and the pht status
   (for the whole cacheline) is returned.
 */
md_addr_t
bpred_lookup_godson(struct bpred_t *pred,	/* branch predictor instance */
                    md_addr_t curpc,           /* current fetch pc */
	            struct bpred_update_t *dir_update_ptr)
{
  struct bpred_btb_ent_t *pbtb = NULL;
  unsigned int index,tag;
  md_addr_t target_pc;
  int i;

  if (!dir_update_ptr)
    panic("no bpred update record");

  pred->lookups++;

  dir_update_ptr->dir.ras = FALSE;
  dir_update_ptr->pdir1 = NULL;
  dir_update_ptr->pdir2 = NULL;
  dir_update_ptr->pmeta = NULL;
  /* Except for jumps, get a pointer to direction-prediction bits */
  switch (pred->class) {
    case BPred2Level:
      dir_update_ptr->pdir1 =
	bpred_dir_lookup (pred->dirpred.twolev, curpc);
      break;
    case BPredNotTaken:
      return curpc + sizeof(md_inst_t);
      /* TODO: add other predictors */
    default:
      panic("bogus predictor class");
  }

   /* Store old shift regs */
   //memcpy(dir_update_ptr->recovery_shiftregs, pred->dirpred.twolev->config.two.shiftregs, sizeof(pred->dirpred.twolev->config.two.shiftregs[0])*pred->dirpred.twolev->config.two.l1size);
  //dir_update_ptr->recovery_shiftregs[0] = pred->dirpred.twolev->config.two.shiftregs[0];

  /* Get a pointer into the BTB */
  index = (curpc >> 5 /* icache->bsize*/) & (pred->btb.sets - 1);

#ifdef PARTIAL_BTB
  tag = ((curpc >> 5 )/ pred->btb.sets) & BTB_TAG_MASK;
#else
  tag = (curpc >> 5) / pred->btb.sets;
#endif

  //btbread->target_pc = ((curpc->curpc>>Nbtbtargetbits) << Nbtbtargetbits) + btb[Nbtbsets-1][curpc_index].target_pc;
  pbtb = &pred->btb.btb_data[index + pred->btb.assoc - 1]; 

  if (pred->btb.assoc > 1)
    {
      index *= pred->btb.assoc;

#if 0
      printf("LOOK:index=%x,tag=%x,addr=%x,%x,target=%x,%x\n",index,tag,
	  pred->btb.btb_data[index].addr,pred->btb.btb_data[index+1].addr,
	  pred->btb.btb_data[index].target,pred->btb.btb_data[index+1].target);
#endif
      /* Now we know the set; look for a PC match */
      for (i = index; i < (index+pred->btb.assoc) ; i++)
	if (pred->btb.btb_data[i].addr == tag)
	  {
	    /* match */
	    pbtb = &pred->btb.btb_data[i];
	    break;
	  }
    }
  else
    {
      pbtb = &pred->btb.btb_data[index];
    }

#ifdef PARTIAL_BTB
  /* BTB hit, so return target if it's a predicted-taken branch */
  target_pc = ((curpc >> BTB_TARGET_BITS ) << BTB_TARGET_BITS ) + pbtb->target;
#else
  target_pc = pbtb->target;
#endif

  return target_pc;
}

void decode_update_ghr(struct bpred_t *pred,/*tick_t now,*/int predict)
{
  struct bpred_dir_t *twolev_dir = pred->dirpred.twolev;

  assert(predict==0 || predict==1);
  /* FIXME: hardcoded now */
  twolev_dir->config.two.shiftregs[0] = 
    ((twolev_dir->config.two.shiftregs[0] << 1) | predict) & twolev_dir->config.two.shift_mask;

  //printf("ghr updated to %x,predict=%x\n",twolev_dir->config.two.shiftregs[0],predict);

  /* maybe we should mask it with shiftwidth here,but original c simulator
   * does not do it too ?*/
}


/* probe a predictor for a next fetch address, the predictor is probed
   with branch address BADDR, the branch target is BTARGET (used for
   static predictors), and OP is the instruction opcode (used to simulate
   predecode bits; a pointer to the predictor state entry (or null for jumps)
   is returned in *DIR_UPDATE_PTR (used for updating predictor state),
   and the non-speculative top-of-stack is returned in stack_recover_idx
   (used for recovering ret-addr stack after mis-predict).  */
md_addr_t				/* predicted branch target addr */
bpred_lookup(struct bpred_t *pred,	/* branch predictor instance */
	     md_addr_t baddr,		/* branch address */
	     md_addr_t btarget,		/* branch target if taken */
	     enum md_opcode op,		/* opcode of instruction */
	     int is_call,		/* non-zero if inst is fn call */
	     int is_return,		/* non-zero if inst is fn return */
	     struct bpred_update_t *dir_update_ptr, /* pred state pointer */
	     int *stack_recover_idx)	/* Non-speculative top-of-stack;
					 * used on mispredict recovery */
{
  struct bpred_btb_ent_t *pbtb = NULL;
  int index, i;

  if (!dir_update_ptr)
    panic("no bpred update record");

  /* if this is not a branch, return not-taken */
  if (!(MD_OP_FLAGS(op) & F_CTRL))
    return 0;

  pred->lookups++;

  dir_update_ptr->dir.ras = FALSE;
  dir_update_ptr->pdir1 = NULL;
  dir_update_ptr->pdir2 = NULL;
  dir_update_ptr->pmeta = NULL;
  /* Except for jumps, get a pointer to direction-prediction bits */
  switch (pred->class) {
    case BPredComb:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
	  char *bimod, *twolev, *meta;
      int l1index, shift_reg;

	  bimod = bpred_dir_lookup (pred->dirpred.bimod, baddr);
	  twolev = bpred_dir_lookup (pred->dirpred.twolev, baddr);
	  meta = bpred_dir_lookup (pred->dirpred.meta, baddr);
	  dir_update_ptr->pmeta = meta;
	  dir_update_ptr->dir.meta  = (*meta >= 2);
	  dir_update_ptr->dir.bimod = (*bimod >= 2);
	  dir_update_ptr->dir.twolev  = (*twolev >= 2);
	  if (*meta >= 2)
	    {
	      dir_update_ptr->pdir1 = twolev;
	      dir_update_ptr->pdir2 = bimod;
	    }
	  else
	    {
	      dir_update_ptr->pdir1 = bimod;
	      dir_update_ptr->pdir2 = twolev;
	    }

        /* also update appropriate L1 history register */
        l1index = (baddr >> MD_BR_SHIFT) & (pred->dirpred.twolev->config.two.l1size - 1);
        shift_reg = (pred->dirpred.twolev->config.two.shiftregs[l1index] << 1) | (!!(*dir_update_ptr->pdir1 >= 2));
        pred->dirpred.twolev->config.two.shiftregs[l1index] = shift_reg & ((1 << pred->dirpred.twolev->config.two.shift_width) - 1);


	}
      break;
    case BPred2Level:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
	  dir_update_ptr->pdir1 =
	    bpred_dir_lookup (pred->dirpred.twolev, baddr);
	}
      break;
    case BPred2bit:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
	  dir_update_ptr->pdir1 =
	    bpred_dir_lookup (pred->dirpred.bimod, baddr);
	}
      break;
    case BPredTaken:
      return btarget;
    case BPredNotTaken:
      if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
	  return baddr + sizeof(md_inst_t);
	}
      else
	{
	  return btarget;
	}
    default:
      panic("bogus predictor class");
  }

  /*
   * We have a stateful predictor, and have gotten a pointer into the
   * direction predictor (except for jumps, for which the ptr is null)
   */

  /* record pre-pop TOS; if this branch is executed speculatively
   * and is squashed, we'll restore the TOS and hope the data
   * wasn't corrupted in the meantime. */
  if (pred->retstack.size)
    *stack_recover_idx = pred->retstack.tos;
  else
    *stack_recover_idx = 0;

   /* Store old shift regs */
   memcpy(dir_update_ptr->recovery_shiftregs, pred->dirpred.twolev->config.two.shiftregs, sizeof(pred->dirpred.twolev->config.two.shiftregs[0])*pred->dirpred.twolev->config.two.l1size);

  /* if this is a return, pop return-address stack */
  if (is_return && pred->retstack.size)
    {
      md_addr_t target = pred->retstack.stack[pred->retstack.tos].target;
      pred->retstack.tos = (pred->retstack.tos + pred->retstack.size - 1)
	                   % pred->retstack.size;
      pred->retstack_pops++;
      dir_update_ptr->dir.ras = TRUE; /* using RAS here */
      /* twenisch - This fixes a bug in the RAS recovery when a 
         speculative return is squashed */
      *stack_recover_idx = pred->retstack.tos;
      return target;
    }

#ifndef RAS_BUG_COMPATIBLE
  /* if function call, push return-address onto return-address stack */
  if (is_call && pred->retstack.size)
    {
      pred->retstack.tos = (pred->retstack.tos + 1)% pred->retstack.size;
      /* twenisch - This fixes a bug in the RAS recovery when a 
         speculative return is squashed */
      *stack_recover_idx = pred->retstack.tos;
      pred->retstack.stack[pred->retstack.tos].target =
	baddr + 2 * sizeof(md_inst_t);
      pred->retstack_pushes++;
    }
#endif /* !RAS_BUG_COMPATIBLE */

  /* not a return. Get a pointer into the BTB */
  index = (baddr >> MD_BR_SHIFT) & (pred->btb.sets - 1);

  if (pred->btb.assoc > 1)
    {
      index *= pred->btb.assoc;

      /* Now we know the set; look for a PC match */
      for (i = index; i < (index+pred->btb.assoc) ; i++)
	if (pred->btb.btb_data[i].addr == baddr)
	  {
	    /* match */
	    pbtb = &pred->btb.btb_data[i];
	    break;
	  }
    }
  else
    {
      pbtb = &pred->btb.btb_data[index];
      if (pbtb->addr != baddr)
	pbtb = NULL;
    }

  /*
   * We now also have a pointer into the BTB for a hit, or NULL otherwise
   */

  /* if this is a jump, ignore predicted direction; we know it's taken. */
  if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) == (F_CTRL|F_UNCOND))
    {
      return (pbtb ? pbtb->target : 1);
    }

  /* otherwise we have a conditional branch */
  if (pbtb == NULL)
    {
      /* BTB miss -- just return a predicted direction */
      return ((*(dir_update_ptr->pdir1) >= 2)
	      ? /* taken */ 1
	      : /* not taken */ 0);
    }
  else
    {
      /* BTB hit, so return target if it's a predicted-taken branch */
      return ((*(dir_update_ptr->pdir1) >= 2)
	      ? /* taken */ pbtb->target
	      : /* not taken */ 0);
    }
}

/* Speculative execution can corrupt the ret-addr stack.  So for each
 * lookup we return the top-of-stack (TOS) at that point; a mispredicted
 * branch, as part of its recovery, restores the TOS using this value --
 * hopefully this uncorrupts the stack. */
void
bpred_recover(struct bpred_t *pred,	/* branch predictor instance */
	      md_addr_t baddr,		/* branch address */
	      int stack_recover_idx,	/* Non-speculative top-of-stack*/
	      struct bpred_update_t * update)
{
  if (pred == NULL)
    return;

  //printf("ghr recover to %x\n",update->recovery_shiftregs[0]);

  //memcpy(pred->dirpred.twolev->config.two.shiftregs, update->recovery_shiftregs, sizeof(pred->dirpred.twolev->config.two.shiftregs[0])*pred->dirpred.twolev->config.two.l1size );
  if (pred->class==BPred2Level) {
    pred->dirpred.twolev->config.two.shiftregs[0] = update->recovery_shiftregs[0];
  }

  if (!pred_perfect) 
    pred->retstack.tos = stack_recover_idx;
}

/* update the branch predictor, only useful for stateful predictors; updates
   entry for instruction type OP at address BADDR.  BTB only gets updated
   for branches which are taken.  Inst was determined to jump to
   address BTARGET and was taken if TAKEN is non-zero.  Predictor
   statistics are updated with result of prediction, indicated by CORRECT and
   PRED_TAKEN, predictor state to be updated is indicated by *DIR_UPDATE_PTR
   (may be NULL for jumps, which shouldn't modify state bits).  Note if
   bpred_update is done speculatively, branch-prediction may get polluted. */
#if 0
void
bpred_update(struct bpred_t *pred,	/* branch predictor instance */
	     md_addr_t baddr,		/* branch address */
	     md_addr_t btarget,		/* resolved branch target */
	     int taken,			/* non-zero if branch was taken */
	     int pred_taken,		/* non-zero if branch was pred taken */
	     int correct,		/* was earlier addr prediction ok? */
	     enum md_opcode op,		/* opcode of instruction */
	     struct bpred_update_t *dir_update_ptr)/* pred state pointer */
{
#else
void
bpred_update(struct bpred_t *pred,	/* branch predictor instance */
             struct inst_descript *rs)
{  
  md_addr_t baddr = rs->regs_PC;	/* branch address */
  /* for mips, regs_NPC of ctrl insts is NOT the target */
  //md_addr_t btarget = rs->regs_NPC;	/* resolved branch target */
  md_addr_t btarget = rs->btarget;	/* resolved branch target */
  int taken = rs->br_taken;		/* non-zero if branch was taken */
  int pred_taken = rs->pred_taken;	/* non-zero if branch was pred taken */
  int correct = !rs->mis_predict;	/* was earlier addr prediction ok? */
  enum md_opcode op = rs->op;		/* opcode of instruction */
  struct bpred_update_t *dir_update_ptr = &rs->dir_update; /* pred state pointer */
#endif
  struct bpred_btb_ent_t *pbtb = NULL;
  //struct bpred_btb_ent_t *lruhead = NULL, *lruitem = NULL;
  int index /*, i*/;
  /* godson only store partial bits in btb */
  md_addr_t tag,btarget_tag;

  /* don't change bpred state for non-branch instructions or if this
   * is a stateless predictor*/
  if (!(MD_OP_FLAGS(op) & F_CTRL))
    return;

  /* Have a branch here */

#if 0
  if (rs->jr_op)
    printf("updating %x for %x\n",btarget,baddr);
#endif

  if (correct)
    pred->addr_hits++;

  if (!!pred_taken == !!taken)
    pred->dir_hits++;
  else
    pred->misses++;

  if (dir_update_ptr->dir.ras)
    {
      pred->used_ras++;
      if (correct)
	pred->ras_hits++;
    }
  else if ((MD_OP_FLAGS(op) & (F_CTRL|F_COND)) == (F_CTRL|F_COND))
    {
      if (dir_update_ptr->dir.meta)
	pred->used_2lev++;
      else
	pred->used_bimod++;
    }

  /* keep stats about JR's; also, but don't change any bpred state for JR's
   * which are returns unless there's no retstack */
  if (MD_IS_INDIR(op))
    {
      pred->jr_seen++;
      if (correct)
    	pred->jr_hits++;

      if (!dir_update_ptr->dir.ras)
	{
	  pred->jr_non_ras_seen++;
	  if (correct)
	    pred->jr_non_ras_hits++;
	}
      else
	{
	  /* return that used the ret-addr stack; no further work to do */
	  return;
	}
    }

  /* Can exit now if this is a stateless predictor */
  if (pred->class == BPredNotTaken || pred->class == BPredTaken || pred_perfect)
    return;

  /*
   * Now we know the branch didn't use the ret-addr stack, and that this
   * is a stateful predictor
   */

   /* Flip the correct bit in the recovery shift regs if neccessary */
  /* in godson2, ghr is updated only for bht_op.
   *
   * if ghrwrite and brerr is valid at the same time,brerr wins.Here ghrwrite
   * is done in service_events,while brerr is done in check_brq,the order is
   * maintained
   */
  if (pred->class == BPred2Level || pred->class == BPredComb)
    {
      //if (pred_taken != taken) 
      if (rs->mis_predict) {
	//int l1index;
	//l1index = (baddr >> MD_BR_SHIFT) & (pred->dirpred.twolev->config.two.l1size - 1);
	if (rs->bht_op) {
	  dir_update_ptr->recovery_shiftregs[0] = dir_update_ptr->recovery_shiftregs[0] ^ 1;
	}else{
	  //dir_update_ptr->recovery_shiftregs[0] = dir_update_ptr->recovery_shiftregs[0];
	}
      }
   }

#if 0
  /* find BTB entry if it's a taken branch (don't allocate for non-taken) */
  if (taken)
    {
#if 0
      index = (baddr >> MD_BR_SHIFT) & (pred->btb.sets - 1);
#else
      /* godson */
      index = (baddr >> 5 ) & (pred->btb.sets - 1);
      tag = ((baddr >> 5 ) / pred->btb.sets) & BTB_TAG_MASK;
      btarget_tag = btarget & BTB_TARGET_MASK; 
#endif

      if (pred->btb.assoc > 1)
	{
	  index *= pred->btb.assoc;

	  /* Now we know the set; look for a PC match; also identify
	   * MRU and LRU items */
	  for (i = index; i < (index+pred->btb.assoc) ; i++)
	  {
		if (pred->btb.btb_data[i].addr == tag)
		{
		  /* match */
		  assert(!pbtb);
		  pbtb = &pred->btb.btb_data[i];
		}

		dassert(pred->btb.btb_data[i].prev
			!= pred->btb.btb_data[i].next);
		if (pred->btb.btb_data[i].prev == NULL)
		{
		  /* this is the head of the lru list, ie current MRU item */
		  dassert(lruhead == NULL);
		  lruhead = &pred->btb.btb_data[i];
		}
		if (pred->btb.btb_data[i].next == NULL)
		{
		  /* this is the tail of the lru list, ie the LRU item */
		  dassert(lruitem == NULL);
		  lruitem = &pred->btb.btb_data[i];
		}
	  }
	  dassert(lruhead && lruitem);

	  if (!pbtb)
	    /* missed in BTB; choose the LRU item in this set as the victim */
	    pbtb = lruitem;
	  /* else hit, and pbtb points to matching BTB entry */

	  /* Update LRU state: selected item, whether selected because it
	   * matched or because it was LRU and selected as a victim, becomes
	   * MRU */
	  if (pbtb != lruhead)
	    {
	      /* this splices out the matched entry... */
	      if (pbtb->prev)
		pbtb->prev->next = pbtb->next;
	      if (pbtb->next)
		pbtb->next->prev = pbtb->prev;
	      /* ...and this puts the matched entry at the head of the list */
	      pbtb->next = lruhead;
	      pbtb->prev = NULL;
	      lruhead->prev = pbtb;
	      dassert(pbtb->prev || pbtb->next);
	      dassert(pbtb->prev != pbtb->next);
	    }
	  /* else pbtb is already MRU item; do nothing */
	}
      else
	pbtb = &pred->btb.btb_data[index];

    }
#endif

  /* update state (but not for jumps) */
  if (rs->bht_op && dir_update_ptr->pdir1)
    {
#if 0
      //printf("update: pc = %x,dir=%x,v=%d\n",rs->regs_PC,dir_update_ptr->pdir1,*dir_update_ptr->pdir1);
      if (taken)
	{
	  if (*dir_update_ptr->pdir1 < 3)
	    ++*dir_update_ptr->pdir1;
	}
      else
	{ /* not taken */
	  if (*dir_update_ptr->pdir1 > 0)
	    --*dir_update_ptr->pdir1;
	}
#else
      //printf("update: pc = %x,current=%d,read=%d,taken=%d,pred=%d,miss=%d\n",rs->regs_PC,*dir_update_ptr->pdir1,dir_update_ptr->st,taken,rs->pred_taken,rs->mis_predict);
      //if (*dir_update_ptr->pdir1 != dir_update_ptr->st) printf("differ\n");
      if (taken)
	{
	  //*dir_update_ptr->pdir1 =(dir_update_ptr->st < 3) ? dir_update_ptr->st + 1 : 3;
	  if (dir_update_ptr->st!=3) *dir_update_ptr->pdir1 = dir_update_ptr->st + 1;
	}
      else
      {
	  //*dir_update_ptr->pdir1 = (dir_update_ptr->st > 0) ? dir_update_ptr->st - 1 : 0;
	  if (dir_update_ptr->st!=0) *dir_update_ptr->pdir1 = dir_update_ptr->st - 1;
      }
#endif
    }

#if 0
  /* combining predictor also updates second predictor and meta predictor */
  /* second direction predictor */
  if (rs->bht_op && dir_update_ptr->pdir2)
    {
      if (taken)
	{
	  if (*dir_update_ptr->pdir2 < 3)
	    ++*dir_update_ptr->pdir2;
	}
      else
	{ /* not taken */
	  if (*dir_update_ptr->pdir2 > 0)
	    --*dir_update_ptr->pdir2;
	}
    }

  /* meta predictor */
  if (dir_update_ptr->pmeta)
    {
      if (dir_update_ptr->dir.bimod != dir_update_ptr->dir.twolev)
	{
	  /* we only update meta predictor if directions were different */
	  if (dir_update_ptr->dir.twolev == (unsigned int)taken)
	    {
	      /* 2-level predictor was correct */
	      if (*dir_update_ptr->pmeta < 3)
		++*dir_update_ptr->pmeta;
	    }
	  else
	    {
	      /* bimodal predictor was correct */
	      if (*dir_update_ptr->pmeta > 0)
		--*dir_update_ptr->pmeta;
	    }
	}
    }
#endif

  /* update BTB (but only for taken branches) */
#if 0
  if (pbtb)
    {
      /* update current information */
      dassert(taken);

      if (pbtb->addr == tag)
	{
	  if (!correct)
	    pbtb->target = btarget_tag;
	}
      else
	{
	  /* enter a new branch in the table */
	  pbtb->addr = tag;
	  pbtb->op = op;
	  pbtb->target = btarget_tag;
	}
    }
#else
  /* godson2 update btb only when jr(not jr31) is mis-predicted,
   * set is randomly chosen.
   */
  if (rs->mis_predict && rs->jr_op && !dir_update_ptr->dir.ras) {
    index = (baddr >> 5 ) & (pred->btb.sets - 1);
#ifdef PARTIAL_BTB
    tag = ((baddr >> 5 ) / pred->btb.sets) & BTB_TAG_MASK;
    btarget_tag = btarget & BTB_TARGET_MASK; 
#endif
    index *= pred->btb.assoc;
    /* random sets */
    index += ((int)sim_cycle & (pred->btb.assoc - 1));
    pbtb = &pred->btb.btb_data[index];
    pbtb->op = op;
#ifdef PARTIAL_BTB
    pbtb->addr = tag;
    pbtb->target = btarget_tag;
#else
    pbtb->addr = (baddr>>5)/pred->btb.sets;
    pbtb->target = btarget;
    //printf("index=%x,addr=%x,target=%x\n",index,baddr,btarget);
#endif
  }
#endif
}

void bpred_backup_stats(struct bpred_t *bp) {
  bp->backup.addr_hits = bp->addr_hits;
  bp->backup.dir_hits = bp->dir_hits;
  bp->backup.used_ras = bp->used_ras;
  bp->backup.used_bimod = bp->used_bimod;
  bp->backup.used_2lev = bp->used_2lev;
  bp->backup.jr_hits = bp->jr_hits;
  bp->backup.jr_seen = bp->jr_seen;
  bp->backup.jr_non_ras_hits = bp->jr_non_ras_hits;
  bp->backup.jr_non_ras_seen = bp->jr_non_ras_seen;
  bp->backup.misses = bp->misses;

  bp->backup.lookups = bp->lookups;
  bp->backup.retstack_pops = bp->retstack_pops;
  bp->backup.retstack_pushes = bp->retstack_pushes;
  bp->backup.ras_hits = bp->ras_hits;

}

void bpred_restore_stats(struct bpred_t *bp) {
  bp->addr_hits = bp->backup.addr_hits;
  bp->dir_hits = bp->backup.dir_hits;
  bp->used_ras = bp->backup.used_ras;
  bp->used_bimod = bp->backup.used_bimod;
  bp->used_2lev = bp->backup.used_2lev;
  bp->jr_hits = bp->backup.jr_hits;
  bp->jr_seen = bp->backup.jr_seen;
  bp->jr_non_ras_hits = bp->backup.jr_non_ras_hits;
  bp->jr_non_ras_seen = bp->backup.jr_non_ras_seen;
  bp->misses = bp->backup.misses;

  bp->lookups = bp->backup.lookups;
  bp->retstack_pops = bp->backup.retstack_pops;
  bp->retstack_pushes = bp->backup.retstack_pushes;
  bp->ras_hits = bp->backup.ras_hits;

}


void
bpred_update_fast(
    md_addr_t baddr,		/* branch address */
    md_addr_t btarget,		/* resolved branch target */
    enum md_opcode op,
    int is_call)		/* non-zero if inst is fn call */
{
    int index, i;
    int taken;
    struct bpred_btb_ent_t *pbtb = NULL;
    /* godson only store partial bits in btb */
    md_addr_t tag,btarget_tag;

    taken = (btarget != (baddr + sizeof(md_inst_t)));

    /* Except for jumps, update direction-prediction bits */
    if ((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
    {
        struct bpred_dir_t *twolev_dir = pred->dirpred.twolev;
        char *twolev;

        /* 2-Level look-up */
        int l1index, l2index, shift_reg;

        /* traverse 2-level tables */
        l1index = (baddr >> MD_BR_SHIFT) & (twolev_dir->config.two.l1size - 1);
        l2index = twolev_dir->config.two.shiftregs[l1index];

        /* update L1 history register */
        shift_reg = (l2index << 1) | taken;
        twolev_dir->config.two.shiftregs[l1index] = shift_reg & twolev_dir->config.two.shift_mask;

	/* godson2 fetch 8 counter per access */
	l2index = ((( baddr >> 5 /* FIXME: icache->bsize */) ^ l2index) << 3 ) +
	           ((baddr & 31) >> 2); /* offset */
        l2index = l2index & (twolev_dir->config.two.l2size - 1);


        /* get a pointer to prediction state information */
        twolev = &twolev_dir->config.two.l2table[l2index];

        if (taken) {
            if (*twolev < 3)
                ++*twolev;
        } else /* not taken */ {
            if (*twolev > 0)
                --*twolev;
        }
    } else if (is_call) {
        pred->retstack.tos = (pred->retstack.tos + 1)% pred->retstack.size;
        pred->retstack.stack[pred->retstack.tos].target = baddr + 2*sizeof(md_inst_t);
    }

  /* godson2 update btb only when jr(not jr31) is mis-predicted,
   * set is randomly chosen. jr31 is handled by caller
   */
  if (op==JR || op==JALR) {
    index = (baddr >> 5 ) & (pred->btb.sets - 1);
#ifdef PARTIAL_BTB
    tag = ((baddr >> 5 ) / pred->btb.sets) & BTB_TAG_MASK;
    btarget_tag = btarget & BTB_TARGET_MASK; 
#else
    tag = baddr;
#endif
    index *= pred->btb.assoc;

    for (i = index; i < (index+pred->btb.assoc) ; i++)
    {
      if (pred->btb.btb_data[i].addr == tag)
      {
	/* hit */
	return;
      }
    }

    /* random sets */
    index += ((int)sim_cycle & (pred->btb.assoc - 1));
    pbtb = &pred->btb.btb_data[index];
    pbtb->op = op;
#ifdef PARTIAL_BTB
    pbtb->addr = tag;
    pbtb->target = btarget_tag;
#else
    pbtb->addr = (baddr >> 5) / pred->btb.sets;
    pbtb->target = btarget;
#endif
  }
}
