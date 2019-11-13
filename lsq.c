#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "mips.h"                                                      
#include "regs.h"
#include "memory.h"
#include "resource.h"
#include "sim.h" 
#include "godson2_cpu.h" 
#include "cache.h"
#include "loader.h"
#include "eventq.h"
#include "bpred.h"
//#include "fetch.h" 
//#include "decode.h"
//#include "issue.h"
//#include "map.h"
//#include "writeback.h"
//#include "commit.h"
#include "eventq.h"
#include "syscall.h"

//#include "lsq.h"
#include "cache2mem.h"

#include "istat.h"

void lsq_init(struct godson2_cpu *st)
{
  st->lsq = (struct load_store_queue *)
    calloc(lsq_ifq_size, sizeof(struct load_store_queue));

  if (!st->lsq) 
    fatal ("out of virtual memory");

  st->lsq_num = 0;
  st->lsq_head1 = 0;
  st->lsq_head = 0;
  st->lsq_tail = 0;

  st->lsq_issuei = st->lsq_dcachei = st->lsq_dtagcmpi = st->lsq_missi = st->lsq_wtbki = -1;
  st->lsq_refill = st->lsq_dcache_refill = st->lsq_dtagcmp_refill = NULL;
}

void lsq_enqueue(struct godson2_cpu *st,struct inst_descript *rs)
{
  rs->lsqid = st->lsq_tail;
  st->lsq[st->lsq_tail].rs = rs;
  st->lsq[st->lsq_tail].op = rs->op;
  st->lsq[st->lsq_tail].tag = rs->tag; /* for brerr cancel */
  st->lsq[st->lsq_tail].state = LSQ_ENTER;
  st->lsq[st->lsq_tail].addr = rs->addr;
  st->lsq[st->lsq_tail].set = CACHE_SET(st->dcache,rs->addr);
  st->lsq[st->lsq_tail].ex = 0;
  st->lsq[st->lsq_tail].loadspec = 0;
  st->lsq[st->lsq_tail].op_load = (MD_OP_FLAGS(rs->op) & F_LOAD)==F_LOAD;
  st->lsq[st->lsq_tail].op_store = (MD_OP_FLAGS(rs->op) & F_STORE)==F_STORE;
  st->lsq_num++;
  st->lsq_tail = (st->lsq_tail+1) % lsq_ifq_size;
}

void lsq_issue(struct godson2_cpu *st, struct inst_descript *rs)
{
  assert(st->lsq[rs->lsqid].rs == rs && st->lsq[rs->lsqid].state==LSQ_ENTER);
  /* no op stalled at memaddr */
  assert(st->lsq_issuei==-1);
  st->lsq[rs->lsqid].state = LSQ_ISSUE;
  st->lsq_issuei = rs->lsqid;
}

static int cache_miss(struct godson2_cpu *st,int i)
{
  return ( !st->lsq[i].fetching && !st->lsq[i].ex && 
      ((st->lsq[i].op_load && st->lsq[i].state==LSQ_READCACHE && st->lsq[i].byterdy!=0xff) ||
       (st->lsq[i].op_store && st->lsq[i].byterdy!=0xff &&
	(st->lsq[i].state == LSQ_READCACHE || st->lsq[i].state==LSQ_WRITEBACK || st->lsq[i].state==LSQ_COMMIT)
       )
      )
      );
}

/* whether lsq[I] is an read op that match the op coming 
 * into lsq[IN]
 */
static int load_match(struct godson2_cpu *st,int in,int i)
{
  return (st->lsq[i].op_load && st->lsq[i].state>=LSQ_READCACHE && DWORD_MATCH(st->lsq[in].paddr,st->lsq[i].paddr));

}
static int store_match(struct godson2_cpu *st,int in,int i)
{
  return (st->lsq[i].op_store && st->lsq[i].state>=LSQ_READCACHE && DWORD_MATCH(st->lsq[in].paddr,st->lsq[i].paddr));
}

/* return a bitmap to indicate which bytes are accessed in a dword */
static unsigned char get_bytemask(int op, int offset)
{
  unsigned char mask;
  switch (op) {
    case SB:
    case LB:
    case LBU:
      mask = 1<<offset;
      break;
    case SH:
    case LH:
    case LHU:
      mask = 3<<offset; /* 11 */
      break;
    case SW:
    case SC:
    case LL:
    case LW:
    case LWC1:
    case SWC1:
      //case LWU:
      mask = 15<<offset; /* 1111 */
      break;
    case SWL:
    case LWL:
      mask = ((1 << ((offset&3) + 1)) - 1) << (offset/4*4);
      break;
    case LWR:
    case SWR:
      mask = ((1<<(4 - (offset&3)))-1) << offset;
      break;
    case LDC1:
    case SDC1:
      mask = 0xff;
      break;
    case MTC1:
    case MFC1:
    case DMTC1:
    case DMFC1:
    case CTC1:
    case CFC1:
      mask = 0x0;
      break;
    default:
      printf("unknow op %s\n",md_op2name[op]);
      mask = 0x0;
  }
  return mask;
}

/* this module include the logic of four pipeline stage:
 *   memaddr,dcache/dtlb,dtagcmp,cp0
 * process them in reverse order
 */
void lsq_stage(struct godson2_cpu *st)
{
  int i,j,next;
  int lsqid;
  md_addr_t addr,paddr;
  int dcachewrite_valid;
  struct cache_blk *blk;

  /* load store queue */
  /* output */
  /* cp0qtail,full,wait_issue_yes,stall_issue_yes ignored */

  /* select item to writeback next cycle,note the case of qfull and qempty */
  i = st->lsq_head;
  next = -1;
  do {
    if (st->lsq[i].state==LSQ_READCACHE && i!=st->lsq_wtbki &&
	(!st->lsq[i].op_load || st->lsq[i].byterdy==0xff)) {
      next = i;
      break;
    }
    i++; 
    if (i==lsq_ifq_size) i=0;
  }while (i!=st->lsq_tail && st->lsq[i].state!=LSQ_EMPTY);

  st->lsq_wtbki = next;

  /* miss */
  /* select item to issue miss request*/
  i = st->lsq_head;
  next = -1;
  do {
    if (cache_miss(st,i)) {
      next = i;
      break;
    }
    i++; 
    if (i==lsq_ifq_size) i=0;
  }while (i!=st->lsq_tail && st->lsq[i].state!=LSQ_EMPTY);

  st->lsq_missi = next;

  /* dcachewrite */
  dcachewrite_valid = st->lsq[st->lsq_head].state==LSQ_COMMIT && 
    st->lsq[st->lsq_head].op_store && st->lsq[st->lsq_head].byterdy == 0xff;
  if (dcachewrite_valid) {
    addr = st->lsq[st->lsq_head].rs->addr;
    /* no drefill,no memref to the same bank*/
#define BANKI(addr) (((addr) & 0x1f) >> 3) 
    if ( !(st->lsq_refill || st->replace_delay>0) && 
		 (st->lsq_issuei==-1 || 
		  !(st->lsq[st->lsq_issuei].op_load && BANKI(st->lsq[st->lsq_issuei].rs->addr) == BANKI(addr))
		 ) 
       ) {
      /* dcachewrite_allow, update cache blk */
      assert(st->lsq[st->lsq_head].blk);
      st->lsq[st->lsq_head].blk->status |= DIRTY;
    }else{
      dcachewrite_valid = 0;
    }
  }

  /* input */
  /* dtagcmp to lsq */

  if (st->lsq_dtagcmp_refill) {
    paddr = st->lsq_dtagcmp_refill->paddr;
	
    if (st->lsq_dtagcmp_refill->replace) {
	  
	  /* reset paddr to the paddr of cache block which has been replace instead of the paddr of the new cache block which will be refilled. */
	  paddr = st->dcache->replace_paddr;
      /* replace */
      i=st->lsq_head;
      /* use do while to deal with qfull case */
	  do {
		if (st->lsq[i].op_store && st->lsq[i].state>=LSQ_READCACHE && BLOCK_MATCH(st->lsq[i].paddr,paddr)) 
		{
		  st->lsq[i].byterdy = ~st->lsq[i].bytemask;
		  st->lsq[i].fetching = 0;
		  st->lsq[i].blk = NULL;
		  st->lsq[i].req = st->lsq[i].op_store ? READ_EX : READ_SHD;
		} 
		/* for processor consistency */
		else if (st->lsq[i].op_load && st->lsq[i].loadspec && st->lsq[i].state == LSQ_WRITEBACK && 
				   BLOCK_MATCH(st->lsq[i].paddr, paddr)) {
		  st->lsq[i].ex = 1;
		  st->sim_load_mispec_rep++;
		  }

		i = i+1;
		if (i==lsq_ifq_size) i = 0;
	  }while(i!=st->lsq_tail);

	} else if(st->lsq_dtagcmp_refill->intervention){
	  
	  int missqid;
	  /* intervention */
	  i=st->lsq_head;
	  /* use do while to deal with qfull case */
	  do {
		if ( st->lsq[i].op_store && st->lsq[i].state>=LSQ_READCACHE && BLOCK_MATCH(st->lsq[i].paddr,paddr)) {
		  st->lsq[i].byterdy = ~st->lsq[i].bytemask;
		  st->lsq[i].fetching = 0;
		  st->lsq[i].blk = NULL;
		  if (st->lsq_dtagcmp_refill->blk){
			st->lsq[i].req = (st->lsq_dtagcmp_refill->blk->status == INVALID) ? READ_EX : UPGRADE;
		  }else{
			st->lsq[i].req = READ_EX;
		  }
		} else if (st->lsq[i].op_load && st->lsq[i].fetching && st->lsq[i].state == LSQ_READCACHE && BLOCK_MATCH(st->lsq[i].paddr,paddr)){
		  /* when replace occured, lsq must not in READCACHE state, but when intervention occured, lsq may still waiting its request to complete. so set fetching to 0 here.*/
		  st->lsq[i].byterdy = ~st->lsq[i].bytemask;
		  st->lsq[i].fetching = 0;
		  st->lsq[i].blk = NULL;
		  st->lsq[i].req = READ_SHD;
		}
		/* for processor consistency */
		else if (st->lsq[i].op_load && st->lsq[i].loadspec && st->lsq[i].state == LSQ_WRITEBACK && 
			BLOCK_MATCH(st->lsq[i].paddr, paddr)) {
		  st->lsq[i].ex = 1;
		  missqid = st->lsq_dtagcmp_refill->missqid;
		  if (st->missq[missqid].req == INTERVENTION_EXC || st->missq[missqid].req == INVALID)
			st->sim_load_mispec_inv++;
		}
		i = i+1;
		if (i==lsq_ifq_size) i = 0;
	  }while(i!=st->lsq_tail);

      missqid = st->lsq_dtagcmp_refill->missqid;
	  
	  if (st->missq[missqid].req == INTERVENTION_SHD || st->missq[missqid].req == INTERVENTION_EXC || st->missq[missqid].req == INVALIDATE){
		/* inform the external request in missq the status of L1 cache and the ack state.*/
		st->missq[st->lsq_dtagcmp_refill->missqid].ack = st->lsq_dtagcmp_refill->ack;
		st->missq[st->lsq_dtagcmp_refill->missqid].cache_status = st->lsq_dtagcmp_refill->cache_status;
		st->missq[st->lsq_dtagcmp_refill->missqid].state = MQ_EXTRDY;
	  } else {
		unsigned long L1_status = st->lsq_dtagcmp_refill->cache_status;
		int cpuid = st->cpuid;
		/* intervention may be invoked by : 
		 * 1.a remote L1 miss hit in local L2, need to modify local L1 status.
		 * 2.L2 miss occured, need to replace L2,so invalidate writeback local L1 status.*/
		if(st->missq[missqid].state == MQ_MODIFY_L1){
		  struct cache_blk* blk = st->missq[missqid].L2_changed_blk;
		  /* missq now is modifying local L1s and waiting for acks */
		  /* check cache status in response and decide if requests needs to be re-sent. */
		  switch (st->missq[missqid].intervention_type){
			case INTERVENTION_SHD:
			  if (L1_status == EXCLUSIVE || L1_status == MODIFIED){/* L2 status must be EXCLUSIVE status here. */
				st->missq[missqid].data_ack_received[cpuid] = 1;
				if (L1_status == MODIFIED)
				  st->missq[missqid].L2_changed_blk->dirty = 1;
			  } else {
				/* INVALID and SHARED L2 should not send INTERVENTION_SHD request. */
				assert((blk->status != INVALID) && (blk->status != SHARED));
				st->missq[missqid].data_intervention_sent[cpuid] = 0;
			  }
			  break;
			case INTERVENTION_EXC:
			  if (blk->status == L1_status || (blk->status == EXCLUSIVE && L1_status == MODIFIED)){
				st->missq[missqid].data_ack_received[cpuid] = 1;
				if (L1_status == MODIFIED)
				  st->missq[missqid].L2_changed_blk->dirty = 1;
			  } else{
				/* L1 cache status should be higher than that of L2 */
				assert((blk->status != INVALID) && !(blk->status == SHARED && (L1_status == EXCLUSIVE || L1_status == MODIFIED)));
				st->missq[missqid].data_intervention_sent[cpuid] = 0;
			  }
			  break;
			case INVALIDATE:
			  if (blk->status == L1_status)/* L2 status must be shared here. */
				st->missq[missqid].data_ack_received[cpuid] = 1;
			  else{
				assert((blk->status != INVALID) && (blk->status != EXCLUSIVE) && !(blk->status == SHARED && (L1_status == EXCLUSIVE || L1_status == MODIFIED)));
				st->missq[missqid].data_intervention_sent[cpuid] = 0;
			  }
			  break;
		  }
		} else if (st->missq[missqid].state == MQ_L2_MISS){
		  struct cache_blk* blk = st->missq[missqid].L2_replace_blk;
		  assert((blk->status != INVALID) && !(blk->status == SHARED && (L1_status == EXCLUSIVE || L1_status == MODIFIED)));
		  if (blk->status == L1_status || (blk->status == EXCLUSIVE && (L1_status == EXCLUSIVE || L1_status == MODIFIED)))
			st->missq[missqid].data_ack_received[cpuid] = 1;
		  else{
			st->missq[missqid].data_intervention_sent[cpuid] = 0;
		  }
		  if (L1_status == MODIFIED)
			blk->dirty = 1;
		}
	  }
	  
	} else {
	  
	  /* refill */
	  i=st->lsq_head;
	  /* use do while to deal with qfull case */
	  do {
		if ((st->lsq[i].op_load || st->lsq[i].op_store) && 
			st->lsq[i].state>=LSQ_READCACHE && DWORD_MATCH(st->lsq[i].paddr,paddr)) {
		  int l1_status;
#ifdef MESI
		  if (st->lsq_dtagcmp_refill->req == READ_SHD)
			l1_status = st->lsq_dtagcmp_refill->cache_status;
		  else
			l1_status = EXCLUSIVE;
#else
		  l1_status = (st->lsq_dtagcmp_refill->req == READ_SHD) ? SHARED : EXCLUSIVE;
#endif
		  if(st->lsq[i].op_store && l1_status == SHARED)
			st->lsq[i].req = UPGRADE;
		  else{
			st->lsq[i].byterdy = 0xff;
			st->lsq[i].fetching = 0;
			if (st->lsq_missi==i) st->lsq_missi=-1;
			st->lsq[i].blk = st->lsq_dtagcmp_refill->blk;
		  }
		}
		i = i+1;
		if (i==lsq_ifq_size) i = 0;
	  }while(i!=st->lsq_tail);
	}

	refill_return_to_free_list(st,st->lsq_dtagcmp_refill);
	st->lsq_dtagcmp_refill = NULL;

  } else if (st->lsq_dtagcmpi!=-1) {
	unsigned char bytemask;
	lsqid = st->lsq_dtagcmpi;
	st->lsq[lsqid].state = LSQ_READCACHE;
	st->lsq[lsqid].fetching = 0;
	bytemask = get_bytemask(st->lsq[lsqid].rs->op,st->lsq[lsqid].addr & 0x7);
	st->lsq[lsqid].bytemask = bytemask;
	st->lsq_dtagcmpi = -1;

	/* forward cache hit loads? */
	if (st->lsq[lsqid].op_load && !st->lsq[lsqid].ex && st->lsq[lsqid].blk) {
	  st->lsq_wtbki = lsqid;
	  /* forward bus not handled yet */
	}

#ifdef ISTAT
	st->lsq[lsqid].rs->dcache_miss = (st->lsq[lsqid].blk==NULL);
#endif

	st->sim_loadcnt += st->lsq[lsqid].op_load;
	st->sim_loadmisscnt += (st->lsq[lsqid].op_load && !st->lsq[lsqid].blk);
	st->sim_storecnt += st->lsq[lsqid].op_store;
	st->sim_storemisscnt += (st->lsq[lsqid].op_store && !st->lsq[lsqid].blk);

	if (st->lsq[lsqid].op_load) {
	  int notfoundmask = 0xff;
	  int matchmask;
	  int end;
	  i=lsqid - 1;
	  if (i<0) i = lsq_ifq_size - 1;

	  end = st->lsq_head1 - 1;
	  if (end<0) end = lsq_ifq_size - 1;
	  /* search back */
	  while (i!=end && (bytemask & notfoundmask)) {
		matchmask = st->lsq[i].bytemask & bytemask & notfoundmask;
		if (matchmask) {
		  if (store_match(st,lsqid,i)) {
			notfoundmask &= ~matchmask;
		  }
		}
		i = i-1;
		if (i<0) i=lsq_ifq_size - 1;
	  }

	  /* for processor consistency */
	  i=lsqid - 1;
	  if (i<0) i = lsq_ifq_size - 1;
	  while (i!=end) {
		if (st->lsq[i].op_load && st->lsq[i].state < LSQ_WRITEBACK) {
		  st->lsq[i].loadspec = 1;
		  break;
		}
		i = i-1;
		if (i<0) i=lsq_ifq_size - 1;
	  }

	  if (notfoundmask!=0xff) {
		st->sim_loadfwdcnt++;
	  }
	  if (st->lsq_wtbki!=lsqid) {
		st->sim_ldwtbkdelaycnt++;
	  }

	  /* cache hit */
	  if (st->lsq[lsqid].blk) {
		st->lsq[lsqid].byterdy = 0xff;
	  } else {
		st->lsq[lsqid].byterdy = ~bytemask | ~notfoundmask;
	  }
	}else if (st->lsq[lsqid].op_store) {
	  int notfoundmask = 0xff;
	  int matchmask;
	  int has_forward = 0;
	  i=lsqid + 1;
	  if (i==lsq_ifq_size) i = 0;
	  while (i!=st->lsq_tail && (bytemask & notfoundmask)) {
		matchmask = st->lsq[i].bytemask & bytemask & notfoundmask;
		if (matchmask) {
		  if (load_match(st,lsqid,i)) {
			st->lsq[i].byterdy |= matchmask;
			has_forward = 1;
			/* mispec */
			if (st->lsq[i].state==LSQ_WRITEBACK || i==st->lsq_wtbki) {
			  /*TODO: miss speculation */
			  st->lsq[i].ex = 1;
			}

		  }

		  /* meet another store, stop forward */
		  if (store_match(st,lsqid,i)) {
			notfoundmask &= ~matchmask;
		  }
		}
		i = i+1;
		if (i==lsq_ifq_size) i = 0;
	  }

	  if (has_forward) 
		st->sim_storefwdcnt++;

	  if (st->lsq[lsqid].blk) {
		st->lsq[lsqid].byterdy = 0xff;
	  } else {
		st->lsq[lsqid].byterdy = ~bytemask;
	  }
	}else{
	  st->lsq[lsqid].byterdy = 0xff;
	}
  }

  if (st->lsq_wtbki!=-1) {
	/* let writeback_stage do its job */
	eventq_queue_event (st, st->lsq[st->lsq_wtbki].rs, sim_cycle + 1 * st->period);
  }

  /* commit insts, update lsq_head */
  i = st->lsq_head1;
  while (st->lsq[i].state==LSQ_DELAY1) {
	/* free the inst */
	fetch_return_to_free_list(st,st->lsq[i].rs);
	st->lsq[i].rs = NULL;
	st->lsq[i].state = LSQ_EMPTY;
	st->lsq_num--;
	i++;
	if (i==lsq_ifq_size) i = 0;
  }
  st->lsq_head1 = i;

  i = st->lsq_head;
  if (dcachewrite_valid || (!st->lsq[i].op_store && st->lsq[i].state==LSQ_COMMIT)) {
	st->lsq[i].state = LSQ_DELAY1;
	i = (i+1) % lsq_ifq_size;
	j = 0;
	while (st->lsq[i].state==LSQ_COMMIT && st->lsq[i].op_load && j<3) {
	  st->lsq[i].state=LSQ_DELAY1;
	  i = (i+1) % lsq_ifq_size;
	  j++;
	}
	st->lsq_head = i;
  }

  /* dcache to dtagcmp */

  if (st->replace_delay>0) st->replace_delay--;

  if (st->lsq_dcache_refill) {
	if (st->lsq_dcache_refill->replace) {
	  /* choose a block to replace,mark it invalid */
	  blk = replace_block(st->dcache,-1,st->lsq_dcache_refill->paddr,st->lsq_dcache_refill->req,0,0, st->lsq_dcache_refill->cache_status);
	  st->lsq_dcache_refill->blk = blk;
	  st->replace_delay = 2;
	} else if (st->lsq_dcache_refill->intervention) {

	  int ack = ACK;
	  int old_cache_status;
	  /* ack could be ACK,ACK_DATA according to the L1 cache state.*/
	  if(st->lsq_dcache_refill->req == INTERVENTION_SHD)
		ack = intervention_shared(st, st->lsq_dcache_refill->paddr,&blk,&old_cache_status);
	  if(st->lsq_dcache_refill->req == INTERVENTION_EXC)
		ack = intervention_exclusive(st, st->lsq_dcache_refill->paddr,&blk,&old_cache_status);
	  if(st->lsq_dcache_refill->req == INVALIDATE)/* no data is expected to return.*/
		invalidate(st, st->lsq_dcache_refill->paddr,&blk,&old_cache_status);
	  st->lsq_dcache_refill->blk = blk;
	  st->lsq_dcache_refill->ack = ack;
	  st->lsq_dcache_refill->cache_status = old_cache_status;
	  st->replace_delay = 2;
	} else {
	  /* doing refill... */
	  st->lsq_dcache_refill->blk = st->dcache->refill_blk;
	  mark_bitmap(st->dcache,(st->lsq_dcache_refill->paddr&0x1f)>>3);
	  if (st->lsq_dcache_refill->cnt == 4){
		int mode;
#ifdef MESI
		if (st->lsq_dcache_refill->req == READ_SHD)
		  mode = st->lsq_dcache_refill->cache_status;
		else
		  mode = EXCLUSIVE;
#else
		mode = (st->lsq_dcache_refill->req == READ_SHD) ? SHARED : EXCLUSIVE;
#endif
		mark_valid(st->dcache,mode);
		st->dcache->refill_blk = NULL;
	  }
	}
	st->lsq_dtagcmp_refill = st->lsq_dcache_refill;
	st->lsq_dcache_refill = NULL;

  }else if (st->lsq_dcachei!=-1) {
	int tlb_hit;

	lsqid = st->lsq_dcachei;
	st->lsq[lsqid].state = LSQ_DTAGCMP;
	st->lsq_dtagcmpi = lsqid;
	st->lsq_dcachei = -1;

	addr = st->lsq[lsqid].rs->addr;

	int is_store = st->lsq[lsqid].op_store;
	int request = 0;

	tlb_hit = dtlb_probe(st->cpuid,addr,&paddr);
	if (!tlb_hit) {
	  /* TODO */
	  st->lsq[st->lsq_dcachei].ex = 1;
	}else{
	  blk = cache_probe(st->dcache,is_store,addr,paddr,&request);
	  st->lsq[lsqid].addr = addr;
	  st->lsq[lsqid].paddr = paddr;
	  st->lsq[lsqid].blk = blk;
	  st->lsq[lsqid].req = request;

          /* lsq_wb_access has been added in writeback stage */
	}

	/* here the order is not important, memaddr will ensure only
	   one,either lsq_dcachei or lsq_dcache_refill will be valid
	   */
  }

  /* memaddr to dcache */
  
  if (st->lsq_refill) {
	st->lsq_dcache_refill = st->lsq_refill;
	st->lsq_refill = NULL;
  } else if (st->lsq_issuei!=-1) {
	st->lsq[st->lsq_issuei].state = LSQ_DCACHE;
	st->lsq_dcachei = st->lsq_issuei;
	st->lsq_issuei = -1;
  }

  /* can't proceed,set busy to prevent next issue */
  if (st->lsq_issuei!=-1) {
	st->lsq[st->lsq_issuei].rs->fu->master->busy = 1;
  }
}

void lsq_cancel_one(struct godson2_cpu *st,int i)
{
  assert(st->lsq[i].state!=LSQ_EMPTY);
  if (st->lsq[i].state==LSQ_ISSUE) st->lsq_issuei=-1;
  else if (st->lsq[i].state==LSQ_DCACHE) st->lsq_dcachei=-1;
  else if (st->lsq[i].state==LSQ_DTAGCMP) st->lsq_dtagcmpi=-1;
  else if (i==st->lsq_missi) st->lsq_missi = -1;

  st->lsq[i].state = LSQ_EMPTY;
  st->lsq_num--;
  st->lsq_tail = i;
}

void lsq_dump(struct godson2_cpu *st)
{
  int i = st->lsq_head1;
  myfprintf(stderr,"head1=%d,head=%d,tail=%d,num=%d\n",st->lsq_head1,st->lsq_head,st->lsq_tail,st->lsq_num);
  myfprintf(stderr,"i st  op  blk   addr  rdy   msk f valid\n");
  while(st->lsq[i].state!=LSQ_EMPTY) {
	myfprintf(stderr,"%2d %2d %5s %1d %8x %3x  %3x   %1d  %1d\n",i,st->lsq[i].state,md_op2name[st->lsq[i].op],(st->lsq[i].blk!=NULL),st->lsq[i].addr,st->lsq[i].byterdy,st->lsq[i].bytemask,st->lsq[i].fetching,st->lsq[i].tag==st->lsq[i].rs->tag);
	i++;
	if (i==lsq_ifq_size) i=0;
  }
}


