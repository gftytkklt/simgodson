/*
 * mshr.h - miss status holding register macros and data structures
 *
 * This file is a part of the SimpleScalar tool suite, and was 
 * written by Alain Kagi, as a part of the Galileo research project.  
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1995, 1996, 1997 by Alain Kagi.
 * Distributed as part of sim-alpha release
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
 */

#define	PG_LEV0	0x0000
#define	PG_LEV1	0x0001
#define	PG_LEV2	0x0002
#define	PG_LEV3	0x0003
#define	PG_LEV4	0x0004
#define	PG_LEV5	0x0005

 
#define UNUSED_MSHREGISTER	-1
#define MAX_REGULAR_MSHRS	64		/* entries used for l/s */
#define MAX_PREFETCH_MSHRS	64		/* entries used for prefetch */
#define MAX_MSHRS	(MAX_REGULAR_MSHRS + MAX_PREFETCH_MSHRS)
#define MAX_TARGETS	16
#define MSHR_FULL_EVENTS_PER_CYCLE 2

 int nregulars;			/* number of allocated regular reg */
 int nprefetches;			/* number of allocated prefetch reg */
 int max_mshr;				/* Largest number of allocated registers,
					   used to optimize search for match */

struct mshregisters
{
    md_addr_t addr;			/* address sent to next cache level */
    md_addr_t addr2;			/* address from prev cache level*/
    md_addr_t vset;			/* Virtual set index buffer for VIPT caches */
    unsigned int cmd;			/* Read if all targets are reads */
    unsigned int size;			/* Number of bytes requested */
    unsigned int vector;		/* Valid bit vector if subblocked request */
    int prefetch;			/* prefetch register? */
    MSHR_STAMP_TYPE stamp;		/* stamp, increment to squash */
    struct cache *cp;			/* back reference for convenience */
  int ntargets;			/* number of allocated targets */
  struct bus *bus;			/* From which bus the response will arrive */
  struct target_table
    {
	tick_t time;		/* time of request */
	struct _cache_access_packet *pkt;		/* packet representing this cache access */
    } target_table[MAX_TARGETS];	/* target descriptors */

    unsigned int table_walk_sm;		/* table walk state machine */

} mshregisters[MAX_MSHRS];

struct mshr_full_event *mshr_full_head;
struct mshr_full_event *mshr_full_tail;

/* access to mshr data structure through cp */
#define MSHR_SET_TLB_STATE(CP,WHICH_REGISTER)	((CP)->mshregisters[(WHICH_REGISTER)].table_walk_sm = PG_LEV1)

#define MSHR_NREGULARS(CP)	((CP)->nregulars)
#define MSHR_MAXREGULARS(CP)	((CP)->max_mshr)
#define MSHR_NPREFETCH(CP)	((CP)->nprefetches)
#define MSHR_ADDR(CP, WHICH_REGISTER)					\
((CP)->mshregisters[(WHICH_REGISTER)].addr)
#define MSHR_ADDR2(CP, WHICH_REGISTER)					\
((CP)->mshregisters[(WHICH_REGISTER)].addr2)
#define MSHR_SIZE(CP, WHICH_REGISTER)					\
((CP)->mshregisters[(WHICH_REGISTER)].size)
#define MSHR_SUBBLOCK_VECTOR(CP, WHICH_REGISTER)			\
((CP)->mshregisters[(WHICH_REGISTER)].vector)
#define MSHR_CMD(CP, WHICH_REGISTER)					\
((CP)->mshregisters[(WHICH_REGISTER)].cmd)
#define MSHR_PREFETCH(CP, WHICH_REGISTER)				\
((CP)->mshregisters[(WHICH_REGISTER)].prefetch)
#define MSHR_STAMP(CP, WHICH_REGISTER)					\
((CP)->mshregisters[(WHICH_REGISTER)].stamp)
#define MSHR_CP(CP, WHICH_REGISTER)					\
((CP)->mshregisters[(WHICH_REGISTER)].cp)
#define MSHR_NTARGETS(CP, WHICH_REGISTER)				\
((CP)->mshregisters[(WHICH_REGISTER)].ntargets)
#define MSHR_BUS(CP, WHICH_REGISTER)				\
((CP)->mshregisters[(WHICH_REGISTER)].bus)
#define MSHR_VSET(CP, WHICH_REGISTER)				\
((CP)->mshregisters[(WHICH_REGISTER)].vset)

#define MSHR_RADDR(CP, REGISTER, TARGET)				\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].pkt->addr)
#define MSHR_TIME(CP, REGISTER, TARGET)					\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].time)
#define MSHR_OBJ(CP, REGISTER, TARGET)					\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].pkt->obj)
#define MSHR_PKT(CP, REGISTER, TARGET)					\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].pkt)
#define MSHR_RELEASE_FN(CP, REGISTER, TARGET)				\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].pkt->release_fn)
#define MSHR_VALID_FN(CP, REGISTER, TARGET)				\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].pkt->valid_fn)
#define MSHR_CACHED_STAMP(CP, REGISTER, TARGET)				\
((CP)->mshregisters[(REGISTER)].target_table[(TARGET)].pkt->stamp)

    /* use only if MSHR_VALID_FN != NULL */
#define MSHR_VALID_TARGET(CP, REGISTER, TARGET)				\
    ((*MSHR_VALID_FN(CP, REGISTER, TARGET))				\
     (MSHR_OBJ(CP, REGISTER, TARGET),				\
      MSHR_CACHED_STAMP(CP, REGISTER, TARGET)))

#define MSHR_INIT_TARGET(CP, REGISTER, TARGET, TIME, PKT)		\
    MSHR_TIME((CP), (REGISTER), (TARGET)) = (TIME);			\
    MSHR_PKT((CP), (REGISTER), (TARGET)) = (PKT);			

    /* access to mshr data structure through mshrp */

#define MSHRP_WALK_STATE(MSHRP)	((MSHRP)->table_walk_sm)


#define MSHRP_CP(MSHRP)		((MSHRP)->cp)
#define MSHRP_ADDR(MSHRP)	((MSHRP)->addr)
#define MSHRP_ADDR2(MSHRP)	((MSHRP)->addr2)
#define MSHRP_CMD(MSHRP)	((MSHRP)->cmd)
#define MSHRP_SIZE(MSHRP)	((MSHRP)->size)
#define MSHRP_SUBBLOCK_VECTOR(MSHRP)	((MSHRP)->vector)
#define MSHRP_PREFETCH(MSHRP)	((MSHRP)->prefetch)
#define MSHRP_STAMP(MSHRP)	((MSHRP)->stamp)
#define MSHRP_NTARGETS(MSHRP)	((MSHRP)->ntargets)
#define MSHRP_VSET(MSHRP)	((MSHRP)->vset)
#define MSHRP_BUS(MSHRP)	((MSHRP)->bus)
#define MSHRP_STAMP(MSHRP)	((MSHRP)->stamp)

#define MSHRP_PKT(MSHRP, TARGET)					\
((MSHRP)->target_table[(TARGET)].pkt)
#define MSHRP_RADDR(MSHRP, TARGET)					\
((MSHRP)->target_table[(TARGET)].pkt->req_addr)
#define MSHRP_TIME(MSHRP, TARGET)					\
((MSHRP)->target_table[(TARGET)].time)
#define MSHRP_OBJ(MSHRP, TARGET)					\
((MSHRP)->target_table[(TARGET)].pkt->obj)
#define MSHRP_RELEASE_FN(MSHRP, TARGET)					\
((MSHRP)->target_table[(TARGET)].pkt->release_fn)
#define MSHRP_VALID_FN(MSHRP, TARGET)					\
((MSHRP)->target_table[(TARGET)].pkt->valid_fn)
#define MSHRP_CACHED_STAMP(MSHRP, TARGET)				\
((MSHRP)->target_table[(TARGET)].pkt->stamp)

    /* use only if MSHRP_VALID_FN != NULL */
#define MSHRP_VALID_TARGET(MSHRP, TARGET)				\
    ((*MSHRP_VALID_FN(MSHRP, TARGET))				\
     (MSHRP_OBJ(MSHRP, TARGET),					\
      MSHRP_CACHED_STAMP(MSHRP, TARGET)))

    /* use only if MSHRP_RELEASE_FN != NULL */
#define MSHRP_RELEASE(MSHRP, TARGET, WHEN)				\
    ((*MSHRP_RELEASE_FN(MSHRP, TARGET))				\
     ((WHEN),							\
      MSHRP_OBJ(MSHRP, TARGET),					\
      MSHRP_CACHED_STAMP(MSHRP, TARGET)))

    /* stats */
    counter_t mshr_hits;
    counter_t mshr_misses;

    /* squashed */
    int TBDsquashed;





















