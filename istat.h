#ifndef __ISTAT_H__
#define __ISTAT_H__

//#define ISTAT

struct inst_stat {
  unsigned long times;
  unsigned long latency[6];
  unsigned long event1; /* brmiss */
  unsigned long event2; /* dcache miss */
  unsigned long event3; /* icache miss */
  unsigned long long ipc_avg;
} ;

extern struct inst_stat *istat;

extern void istat_init(void);
extern int save_istat(char *filename);
extern void save_pcinfo(struct inst_descript *rs);
extern void istat_add_sample(int committed);

#define IPC_RANGE_N 64

#endif

