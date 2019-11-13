#ifndef __NOC_H__
#define __NOC_H__

#include "host.h"

/************************* network on chip part *************************/

/* channel direction */
enum {HOME = 0, UP = 1, DOWN = 2, LEFT = 3, RIGHT = 4, NUM_DIRECTIONS = 5, NOUGHT = 6};

/* channel type */
enum {REQ, INVN, WTBK, RESP, NUM_TYPES};
enum {FLIT_HEAD, FLIT_BODY, FLIT_TAIL};
#define PACK_SIZE 2 

struct inputbuffer {
  int valid;
  int source_channel;
  int index;
  tick_t time_stamp;
  
  int dest_x;
  int dest_y;
  int req;
  int cache_status;/* record L1 cache status for ack. */
  int qid;
  int cpuid;
  int inst;
  md_addr_t paddr;
  int flit_type;
};

struct router_t {
  /* input buffer */
  struct inputbuffer **ingress[NUM_TYPES][NUM_DIRECTIONS];

  /* priority */
  int priority[NUM_TYPES];

  /* input buffer variables */
  int ingress_head[NUM_TYPES][NUM_DIRECTIONS];
  int ingress_tail[NUM_TYPES][NUM_DIRECTIONS];
  int ingress_num[NUM_TYPES][NUM_DIRECTIONS];
  
  /* output direction variables */
  int egress_credit[NUM_TYPES][NUM_DIRECTIONS];
  int egress_last_grant[NUM_TYPES][NUM_DIRECTIONS];
  int used[NUM_TYPES][NUM_DIRECTIONS];
  int inputdirec[NUM_TYPES][NUM_DIRECTIONS];
  struct inputbuffer *egress_grant[NUM_TYPES][NUM_DIRECTIONS];
  
  /* router x-y coordinate */
  int home_x;
  int home_y;

  int active;

  /* NOC statistics */
  counter_t pack_sent[NUM_TYPES];
  counter_t pack_latency[NUM_TYPES];
  double    avg_pack_latency[NUM_TYPES];
  double    avg_latency;

  /* access counters added for our power model */
  counter_t input_buffer_read_access[NUM_TYPES][NUM_DIRECTIONS];
  counter_t input_buffer_write_access[NUM_TYPES][NUM_DIRECTIONS];
  counter_t crossbar_access[NUM_TYPES];

};

/* network on chip */
void noc_init();
#ifdef ROUTER_SPLIT
void noc_stage(tick_t now, struct godson2_cpu *st);
#else
void noc_stage(struct godson2_cpu *st);
#endif
int noc_request(struct godson2_cpu *st, int chan_type, int request,int cache_status, int qid, int cpuid, md_addr_t paddr,int inst);
void update_credit(tick_t now, int home_x, int home_y, int type, int direction);

#endif
