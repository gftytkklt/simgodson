/* for GDB */
#include "godson2_cpu.h"
#include "sim.h"

static const char* cache_stat[] = {"UNTOUCHED", "INVALID", "SHARED", "EXCLUSIVE", "DIRTY", "DIRTY_INVALID", "", "MODIFIED"};
static const char* missq_stat[] = {"MQ_EMPTY",      "MQ_L1_MISS",   "MQ_READ_L2",   "MQ_MODIFY_L1",
								   "MQ_REPLACE_L1", "MQ_EXTRDY",    "MQ_REFILL_L1", "MQ_L2_MISS",
								   "MQ_MEMREF",     "MQ_REFILL_L2", "MQ_DELAY1",    "MQ_DELAY2",
								   "MQ_DELAY3",     "MQ_STATE_NUM"};
void pinst()
{
  int i;
  for (i = 0; i < total_cpus; i++)
	printf("%ld ", (long)cpus[i].sim_commit_insn);
  printf("\n");
}

void pmissqnum()
{
  int i;
  for (i = 0; i < total_cpus; i++)
	printf("%d ", cpus[i].missq_num);
  printf("\n");
}

void pcache_status(md_addr_t paddr, int cpuid)
{
  struct godson2_cpu *st = &cpus[PADDR_OWNER(paddr)];
  struct cache_blk *blk;
  md_addr_t tag = CACHE_TAG(st->cache_dl2, paddr);
  md_addr_t set = CACHE_SET(st->cache_dl2, paddr);
  int find = 0, i;
  char d_dir[256];
  char i_dir[256];
  char tmp[10];

  memset(d_dir, 0, 256);
  memset(i_dir, 0, 256);
  for (blk = st->cache_dl2->sets[set].way_head; blk; blk = blk->way_next) {
	if (blk->tag == tag) {
	  for (i = 0; i < total_cpus; i++) {
		sprintf(tmp, "%d ", blk->data_directory[i]);
		strcat(d_dir, tmp);
		sprintf(tmp, "%d ", blk->inst_directory[i]);
		strcat(i_dir, tmp);
	  }
	  printf("L2 status: paddr = %10p, status = %s(%d), dirty = %d\n", (void *)paddr, cache_stat[blk->status], blk->status, blk->dirty);
	  printf("d_dir:%s  i_dir:%s\n", d_dir, i_dir);
	  find = 1;
	}
  }
  if (!find)
	printf("Not find in L2$\n");

  find = 0;
  st = &cpus[cpuid];
  tag = CACHE_TAG(st->dcache, paddr);
  set = CACHE_SET(st->dcache, paddr);
  for (blk = st->dcache->sets[set].way_head; blk; blk = blk->way_next) {
	if (blk->tag == tag) {
	  printf("L1 status: paddr = %10p, status = %s(%d), dirty = %d\n", (void *)paddr, cache_stat[blk->status], blk->status, blk->dirty);
	  find = 1;
	}
  }
  if (!find)
	printf("Not find in L1 d$\n");
  
  find = 0;
  st = &cpus[cpuid];
  tag = CACHE_TAG(st->icache, paddr);
  set = CACHE_SET(st->icache, paddr);
  for (blk = st->icache->sets[set].way_head; blk; blk = blk->way_next) {
    if (blk->tag == tag) {
	  printf("L1 status: paddr = %10p, status = %s(%d), dirty = %d\n", (void *)paddr, cache_stat[blk->status], blk->status, blk->dirty);
	  find = 1;
    }
  }
  if (!find)
	printf("Not find in L1 i$\n");
}

void pmissq(int cpuid)
{
  int i;
  for (i = 0; i < missq_ifq_size; i++) {
	printf("%d: paddr = %10p, status = %s(%d), cpuid = %d\n", i, (void *)cpus[cpuid].missq[i].paddr, missq_stat[cpus[cpuid].missq[i].state], cpus[cpuid].missq[i].state, cpus[cpuid].missq[i].cpuid);
  }
}
