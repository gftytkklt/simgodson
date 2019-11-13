#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "mips.h"                                                      
#include "regs.h"
#include "memory.h"
#include "resource.h"
#include "sim.h" 
#include "cache.h"
#include "loader.h"
#include "eventq.h"
#include "bpred.h"
#include "fetch.h" 
#include "decode.h"
#include "issue.h"
#include "map.h"
#include "writeback.h"
#include "commit.h"
#include "eventq.h"
#include "syscall.h"
#include "ptrace.h"

#include "cache2mem.h"
#include "lsq.h"

#include "istat.h"

struct inst_stat *istat = NULL;

int sum_ipc=0;
int commit_ipc[IPC_RANGE_N];
int commit_ipc_head = 0;

void istat_init(void)
{
  int i;

  istat = calloc(1,cpus[0].ld_text_size/sizeof(md_inst_t) * sizeof(struct inst_stat));
  if (!istat) {
    myfprintf(stderr,"failed to allocate istat array!\n");
    exit(-1);
  }

  for (i=0;i<IPC_RANGE_N;i++) {
    commit_ipc[i] = 0;
  }
}

void istat_add_sample(int committed)
{
  sum_ipc -= commit_ipc[commit_ipc_head];
  commit_ipc[commit_ipc_head] = committed;
  sum_ipc += committed;
  commit_ipc_head = (commit_ipc_head==IPC_RANGE_N-1)? 0 : commit_ipc_head+1;
}

void save_pcinfo(struct inst_descript *rs)
{
#if 0
  int index;

  assert(rs->regs_PC>= cpus[0].ld_text_base && rs->regs_PC < cpus[0].ld_text_base + cpus[0].ld_text_size);
  index = (rs->regs_PC - cpus[0].ld_text_base) / sizeof(md_inst_t);

  istat[index].times++;

  istat[index].latency[0] += rs->fetch_latency;
  istat[index].latency[1] += rs->decode_stamp - rs->time_stamp;
  istat[index].latency[2] += rs->map_stamp - rs->decode_stamp;
  istat[index].latency[3] += rs->issue_stamp - rs->map_stamp;
  istat[index].latency[4] += rs->writeback_stamp - rs->issue_stamp;
  istat[index].latency[5] += sim_cycle - rs->writeback_stamp;

  if (rs->mis_predict) {
    istat[index].event1++;
  }
  if (rs->dcache_miss) {
    istat[index].event2++;
  }
  if (rs->icache_miss) {
    istat[index].event3++;
  }

  istat[index].ipc_avg += sum_ipc;
#endif
}

int save_istat(char *filename)
{
  int i;
  int count = 0;

  FILE *f;

  f = fopen(filename,"w+");
  if (!f) {
    myfprintf(stderr,"failed to open istat file %s\n",filename);
    return -1;
  }

  for (i=0;i<cpus[0].ld_text_size/sizeof(md_inst_t);i++) {
    if (istat[i].times>0) {
      fprintf(f,"%8x %8u ",(unsigned int)cpus[0].ld_text_base + i * sizeof(md_inst_t),(unsigned int)istat[i].times);
      fprintf(f,"L:%3.1f %3.1f %3.1f %3.1f %3.1f %3.1f,E %8ld %8ld %8ld IPC %3.1f\n",
	  ((double)(istat[i].latency[0]))/istat[i].times,
	  ((double)(istat[i].latency[1]))/istat[i].times,
	  ((double)(istat[i].latency[2]))/istat[i].times,
	  ((double)(istat[i].latency[3]))/istat[i].times,
	  ((double)(istat[i].latency[4]))/istat[i].times,
	  ((double)(istat[i].latency[5]))/istat[i].times,
	  istat[i].event1,
	  istat[i].event2,
	  istat[i].event3,
	  istat[i].ipc_avg/istat[i].times/(double)IPC_RANGE_N);
      count++;
    }
  } 
  fclose(f);
  printf("%d inst executed out of total %d\n",count,cpus[0].ld_text_size/sizeof(md_inst_t));
  return 0;
}


