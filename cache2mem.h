#ifndef __CACHE2MEM__H__
#define __CACHE2MEM__H__

#if 0

#define BLOCK_MATCH(a,b) (((a) & ~0x1f) == ((b) & ~0x1f))

enum miss_state {
  MQ_EMPTY,MQ_MISS,MQ_VCMISS,MQ_VCHIT,MQ_MEMREF,MQ_DELAY1,MQ_DELAY2,MQ_DELAY3
};

struct miss_queue {
  enum miss_state state;
  int  set;
  md_addr_t paddr;
  int  inst;
  int  memcnt;
  int  cachecnt;
};

struct writeback_queue {
  int valid;
  int w;
  md_addr_t paddr;
};

struct refill_packet {
  int valid;
  int replace;
  int set;
  int paddr;
  int cnt;
  struct cache_blk *blk;
  struct refill_packet *next;
};

struct memory_queue {
  int read;
  int qid;
  int count;
  struct memory_queue *next;
};

#define REFILL_FREE_LIST_SIZE   8

extern struct refill_packet *refill_free_list;

extern counter_t mread_count;
extern counter_t mwrite_count;
extern counter_t memq_busy_count;
extern counter_t dmemread_count;
extern counter_t imemread_count;

extern int missq_ifq_size;
extern int wtbkq_ifq_size;
extern struct miss_queue *missq;
extern struct writeback_queue *wtbkq;

extern int memq_ifq_size;
extern int memq_num;
extern struct memory_queue *memq_head,*memq_free_list;

extern struct refill_bus refill;

extern int mem_read_first_delay;
extern int mem_read_interval_delay;
extern int mem_write_delay;

extern void cache2mem_init(void);
extern void cache2mem(void);
extern void check_memq(void);
extern void wtbkq_enter(md_addr_t paddr,int w);

extern struct refill_packet *refill_get_free_list(void);
extern void refill_return_to_free_list(struct refill_packet *temp);

#endif
void missq_change_state(struct godson2_cpu* st,int qid,int new_state);
#endif
