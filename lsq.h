#ifndef LSQ_H
#define LSQ_H

#if 0

#define DWORD_MATCH(a1,a2) (((a1)|0x7) == ((a2)|0x7))

enum lsq_state { LSQ_EMPTY,LSQ_ENTER,LSQ_ISSUE,LSQ_DCACHE,LSQ_DTAGCMP,LSQ_READCACHE,LSQ_WRITEBACK,LSQ_COMMIT,LSQ_DELAY1 };

struct load_store_queue {
  struct inst_descript *rs;
  enum lsq_state state;
  int tag;
  int op;
  int op_load;
  int op_store;
  md_addr_t addr;
  md_addr_t paddr;
  struct cache_blk *blk;
  int ex;
  int set;
  int way;
  int cachehit;
  unsigned char byterdy;
  unsigned char bytemask;
  int fetching;
};


extern struct load_store_queue *lsq;

extern int lsq_head1,lsq_head,lsq_tail,lsq_num;

extern int lsq_ifq_size;

/* index of lsq item in certain states,at most one item for each of these */
extern int lsq_issuei,lsq_dcachei,lsq_dtagcmpi,lsq_missi,lsq_wtbki;

/* refill/replace operation in dcache/dtagcmp stage */
extern struct refill_packet *lsq_refill,*lsq_dcache_refill,*lsq_dtagcmp_refill;

extern counter_t sim_loadcnt,sim_storecnt,sim_loadmisscnt,sim_storemisscnt;
extern counter_t sim_loadfwdcnt,sim_storefwdcnt,sim_ldwtbkdelaycnt;

extern counter_t lsq_count,lsq_fcount;

extern void lsq_enqueue(struct inst_descript *rs);
extern void lsq_issue(struct inst_descript *rs);
extern void lsq_init(void);
extern void lsq_stage(void);
extern void lsq_cancel_one(int i);
extern void lsq_dump(void);

#endif

#endif
