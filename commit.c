/*
* commit.c - commit stage implementation
*
* This file is part of the godson2 simulator tool suite.
*
* Copyright (C) 2004 by Fuxin Zhang, ICT.
*
* This source file is distributed "as is" in the hope that it will be
* useful.  It is distributed with no warranty, and no author or
* distributor accepts any responsibility for the consequences of its
* use. 
*
* Everyone is granted permission to copy, modify and redistribute
* this source file under the following conditions:
*
*    This tool set is distributed for non-commercial use only. 
*    Please contact the maintainer for restrictions applying to 
*    commercial use of these tools.
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
*/

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
#include "ptrace.h"
//#include "lsq.h"
#include "cache2mem.h"
#include "istat.h"
#include "sampling.h"

//#define CROSS_VERIFY

#ifdef CROSS_VERIFY

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
                                                                                                                              
#define SHM_KEY  0x6666
#define SEM_KEY  0x6688
                                                                                                                              
#define N 16384 /* generete this many pc then wait for consumer */
                                                                                                                              
static int shmid = 0;
static int *shm =NULL; /* share mem array to hold commit pc */
                                                                                                                              
/* we use two semaphores to sync:
 *   sem 0: set to zero at init,add by 1 when N instruction is collected
 *          consumer try to decrease it before using the shm
 *   sem 1: set to zero at init,add by 1 when N instruction is used by consumer
 *          producer try to decrease it before generating next batch pcs.
 */
static int semid = 0;

static unsigned int mypc[N];
static unsigned long long mycycle[N];
static int pc_index=0; /* next to write index of pc array */
static int batch=0; 

void cross_verify_init(void)
{
  /* create shm and semaphore */
  shmid = shmget(SHM_KEY,N*8,IPC_CREAT | 0777);
  if (shmid==-1) {
    myfprintf(stderr,"Failed to create shm\n");
    /* try another? */
    exit(-1);
  }
  shm = (int*)shmat(shmid,NULL,0);
  if (shm==(int*)-1) {
    myfprintf(stderr,"Failed to attach shm\n");
    exit(-1);
  }

  semid = semget(SEM_KEY,2,IPC_CREAT | 0777);
  if (semid==-1) {
    myfprintf(stderr,"Failed to create sem\n");
    shmdt(shm);
    exit(-1);
  }

}

/* we have used N insts,tell producer */
static void signal_producer(void)
{
  struct sembuf sbuf[2];
  sbuf[0].sem_num = 1;
  sbuf[0].sem_op = 1;
  sbuf[0].sem_flg = SEM_UNDO;

  semop(semid,sbuf,1);
}

static void wait_producer(void)
{
  struct sembuf sbuf[2];
  sbuf[0].sem_num = 0;
  sbuf[0].sem_op = -1;
  sbuf[0].sem_flg = SEM_UNDO;

  semop(semid,sbuf,1);
}

void add_new_pc(struct inst_descript *rs)
{
  int i,j;
  static md_addr_t last_pc=-1;
#if 0

  mypc[pc_index] = npc;
  mycycle[pc_index] = godson_clock;
  pc_index++;
  if (pc_index==N) {
    wait_producer();
    for (i=0;i<N;i++) {
      if (mypc[i]!=shm[i]) {
	printf("PC mismatch!: expected: %x, found %x at cycle %lld,batch %d[%d]\n",shm[i],mypc[i],mycycle[i],batch,i);
	for (j=20;j>=0;j--)
	  if (i-j>0) printf("pc %x:%x\n",shm[i-j],mypc[i-j]);
        godson_assert(0,"");
	break;
      }
    }
    batch++;
    pc_index = 0;
    signal_producer();
  }
#else
  if (rs->regs_PC==last_pc) {
    return;
  }else{
    last_pc = rs->regs_PC;
  }

  if (pc_index==0) {
    wait_producer();
  }
  if (shm[pc_index]!=rs->regs_PC) {
    printf("PC mismatch!: expected: %x, found %x at cycle %lld,batch %d[%d]\n",shm[pc_index],rs->regs_PC,sim_cycle,batch,pc_index);
    for (j=20;j>=0;j-=2)
      if (pc_index-j>0) printf("pc %x,v=%x\n",shm[pc_index-j],shm[pc_index-j+1]);
    exit_now(-1);
  }
  pc_index++;
  if (rs->regn!=-1) {
    if (shm[pc_index]!=rs->regv) {
      printf("Dest reg %d mismatch!: pc %x,expected: %x, found %x,batch %d[%d]\n",rs->regn,rs->regs_PC,shm[pc_index],rs->regv,batch,pc_index);
      //exit_now(-1);
    }
  }
  pc_index++;

  if (pc_index==N) {
    batch++;
    pc_index = 0;
    signal_producer();
  }
#endif
}

#endif

/* recover changes to arch state for exceptions */
void recover_data(struct godson2_cpu *st,struct inst_descript *current)
{
  int i;

  if (current->spec_level!=0) return;

  /* restore old value */
  for (i=0;i<current->ti;i++) {
    //printf("pc %x:",current->regs_PC);
    switch (current->temp_save[i].t) {
      case 0: /* fix reg */
	//printf("recover reg %d,v=%x\n",current->temp_save[i].n,current->temp_save[i].l);
	assert(current->temp_save[i].n>=0 && current->temp_save[i].n<32);
	st->regs.regs_R[current->temp_save[i].n] = current->temp_save[i].l;
	break;
      case 1: /* stores */
	//printf("recover mem %x,v=%x\n",current->temp_save[i].addr,current->temp_save[i].l);
	mem_access(st->mem,Write,current->temp_save[i].addr,&current->temp_save[i].l,sizeof(word_t));
	break;
      case 2: /* sfloat */
	//printf("recover freg %d,v=%x\n",current->temp_save[i].n,current->temp_save[i].f);
	assert(current->temp_save[i].n>=0 && current->temp_save[i].n<32);
	st->regs.regs_F.f[current->temp_save[i].n] = current->temp_save[i].f;
	break;
      case 3: /* dfloat */
	//printf("recover dreg %d,v=%x\n",current->temp_save[i].n,current->temp_save[i].d);
	assert(current->temp_save[i].n>=0 && current->temp_save[i].n<32);
	st->regs.regs_F.d[current->temp_save[i].n] = current->temp_save[i].d;
	break;
      case 4:
	//printf("recover hi,v=%x\n",current->temp_save[i].l);
	st->regs.regs_C.hi = current->temp_save[i].l;
	break;
      case 5:
	//printf("recover lo,v=%x\n",current->temp_save[i].l);
	st->regs.regs_C.lo = current->temp_save[i].l;
	break;
      case 6:
	//printf("recover fcc,v=%x\n",current->temp_save[i].l);
	st->regs.regs_C.fcc = current->temp_save[i].l;
	break;
      case 7:
	//printf("recover freg %d,v=%x\n",current->temp_save[i].n,current->temp_save[i].f);
	assert(current->temp_save[i].n>=0 && current->temp_save[i].n<32);
	st->regs.regs_F.l[current->temp_save[i].n] = current->temp_save[i].l;
	break;
      default:
	assert(0);
    } /* switch */
  } /* for */
}

/* flush pipeline for exceptions */
void pipeline_flush(struct godson2_cpu *st, struct inst_descript *rs)
{
  int i, roq_index = st->roq_tail;
  struct inst_descript *current;
  md_addr_t recover_PC = (rs->bd ? (rs->regs_PC - 4):rs->regs_PC);

  //myfprintf(stdout,"flush at %x: roq h %d,t %d,n %d,bd=%d\n",rs->regs_PC,roq_head,roq_tail,roq_num,rs->bd);
  
  //if (rs->regs_PC==0x400f96c) roq_dump();
  /* we need to do this first! there are committed instruction in decode_data
   * recover them first
   */
  tracer_recover(st,0,recover_PC);

  /* recover from the tail of the RUU towards the head until the branch index
     is reached, this direction ensures that the LSQ can be synchronized with
     the RUU */

  /* go to first element to squash */
  roq_index = (roq_index + (roq_ifq_size-1)) % roq_ifq_size;

  /* traverse to older insts until the (delay slot of) mispredicted branch is
   * encountered 
   */
  while (st->roq_num>0)
    {
      current = st->roq[roq_index];

      /* recover any resources used by this roq operation */
      for (i=0; i<MAX_ODEPS; i++)
	{
	  RSLINK_FREE_LIST(current->odep_list[i]);
	  /* blow away the consuming op list */
	  current->odep_list[i] = NULL;
	}

      /* squash this RUU entry */
      current->tag++;

      /* free possible used non-pipeline unit */
      current->fu->busy = 0;

      /* free brq item */
      if (MD_OP_FLAGS(current->op) & F_CTRL) {
    	st->brq[current->brqid] = NULL;
    	st->brq_tail = (st->brq_tail + brq_ifq_size - 1) % brq_ifq_size;
	    st->brq_num --;
      }

      /* free lsq item */
      if (st->dcache && (MD_OP_FLAGS(current->op) & F_MEM)) {
    	lsq_cancel_one(st,current->lsqid);
      }

      /* free rename registers */
      st->int_rename_reg_num -= current->used_int_rename_reg;
      st->fp_rename_reg_num  -= current->used_fp_rename_reg;

      /* add access counters for our power model */
      st->commit_access++;
      if(MD_OP_FLAGS(rs->op) & F_MEM){
        st->lsq_commit_access++;
      }
      if(MD_OP_FLAGS(rs->op) & F_FCOMP){
        st->fp_commit_access++;
      }else{
        st->fix_commit_access++;
      }

      /* free issue queue item */
      if (current->queued) {
    	if (current->in_intq) 
    	  st->int_issue_num --;
    	else
    	  st->fp_issue_num --;
      }

      recover_data(st,current);

#     if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
      /* indicate in pipetrace that this instruction was squashed */
      ptrace_endinst(current->ptrace_seq);
#     endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )

      /* free the instruction data */
      fetch_return_to_free_list(st,current);

      st->roq[roq_index] = NULL;

      /* go to next earlier slot in the roq */
      roq_index = (roq_index + (roq_ifq_size-1)) % roq_ifq_size;
      st->roq_num--;
    }

  /* reset head/tail pointers */
  st->roq_head = st->roq_tail = 0;

  //bpred_recover(pred, rs->regs_PC, rs->stack_recover_idx, NULL);

  st->ghr_valid = FALSE;

  clear_create_vector(st);

  st->fetch_istall_buf.stall |= BRANCH_STALL;
}

/*
 *  instruction retirement pipeline stage
 */
void
commit_stage_init(struct godson2_cpu *st)
{
#ifdef CROSS_VERIFY
  cross_verify_init();
#endif
}

struct inst_descript *last_commit_rs;

/* return committed inst number */
int
commit_stage(struct godson2_cpu *st)
{
# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
  int i, events;
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
  static int load_miss_penalty = 0;
  int lat, committed = 0;

  if (load_miss_penalty>0) {
     load_miss_penalty--; 
  }

  /* all values must be retired to the architected reg file in program order */
  while (st->roq_num > 0 && committed < commit_width && load_miss_penalty==0 )
    {
      struct inst_descript *rs = st->roq[st->roq_head];

      if (rs->trap) {
        fatal("TRAP commit?? inst at %x, sim_cycle is %d\n",rs->regs_PC,sim_cycle);
      }

      if (!rs->completed){
	  /* at least roq entry must be complete */
	    break;
	  }

      /* invalidate roq operation instance */
      rs->tag++;

#ifdef ISTAT
      save_pcinfo(rs);     
#endif

	  last_commit_rs = rs;

      /* print retirement trace if in verbose mode */
      if (0 /*verbose && (sim_pop_insn % opt_inst_interval == 0)*/) {
        myfprintf(stderr, "cpu%d %10n @ 0x%08p: ", st->cpuid, st->sim_commit_insn, rs->regs_PC);
 	    md_print_insn(rs->IR, rs->regs_PC, stderr);
	    if (MD_OP_FLAGS(rs->op) & F_MEM){
		  //md_addr_t paddr;
		  //dtlb_probe(st->mem, rs->addr, &paddr);
	      myfprintf(stderr, "  paddr: 0x%08p",st->lsq[rs->lsqid].paddr/*rs->addr*/);
		}
	    fprintf(stderr, "\n");
	  /* fflush(stderr); */
	  }

      /* update lsq state*/
      if ((MD_OP_FLAGS(rs->op) & F_MEM) && st->dcache) {
    	if (st->lsq[rs->lsqid].ex) {
	  //load_miss_penalty = missspec_penalty;
          pipeline_flush(st,rs);
	      st->sim_missspec_load++;
    	  return committed;
	    }
    	st->lsq[rs->lsqid].state=LSQ_COMMIT;
      }

      /* free brq item */
      if (MD_OP_FLAGS(rs->op) & F_CTRL) {
    	assert(st->brq_head == rs->brqid);
    	st->brq[st->brq_head] = NULL;
    	st->brq_head = (st->brq_head + 1) % brq_ifq_size;
	    st->brq_num --;

      }

      /* free rename register */
      st->int_rename_reg_num -= rs->used_int_rename_reg;
      st->fp_rename_reg_num -= rs->used_fp_rename_reg;

      /* add access counters for our power model */
      st->commit_access++;
      if(MD_OP_FLAGS(rs->op) & F_MEM){
        st->lsq_commit_access++;
      }
      if(MD_OP_FLAGS(rs->op) & F_FCOMP){
        st->fp_commit_access++;
      }else{
        st->fix_commit_access++;
      }

      //printf("commit insn pc is 0x%x\n",st->roq[st->roq_head]->regs_PC);
	  
      /* commit head entry of roq */
      st->roq[st->roq_head] = NULL;
      st->roq_head = (st->roq_head + 1) % roq_ifq_size;
      st->roq_num--;

      st->sim_commit_insn++;

      /* one more instruction committed to architected state */
      committed++;

      /* one more non-speculative instruction executed */
      //if (st->simulator_state == MEASURING) 
	  {
    	st->sim_meas_insn++;
      }
      st->sim_detail_insn++;
      if (rs->op != SPLITMUL && rs->op != SPLITDIV) {
	    st->sim_pop_insn++;
      }
      if (MD_OP_FLAGS(rs->op) & F_CTRL) {
    	st->sim_num_branches++;
      }
      st->last_non_spec_rs = rs;

#ifdef CROSS_VERIFY
      if (rs->op != SPLITMUL && rs->op != SPLITDIV) {
    	add_new_pc(rs);
      }
#endif

#if 1
      /* print retirement trace if in verbose mode */
      if (verbose && (st->sim_pop_insn % opt_inst_interval == 0)) {
        if (rs->op != SPLITMUL && rs->op != SPLITDIV) {
          myfprintf(stdout, "%10n @ 0x%08p: ", st->sim_pop_insn, rs->regs_PC);
          md_print_insn(rs->IR, rs->regs_PC, stdout);
          fprintf(stdout, "\n");
        }
    	/* fflush(stderr); */
      }
#endif

#if 0
      /* update memory access stats */
      if (MD_OP_FLAGS(rs->op) & F_MEM){
	    sim_num_refs++;

	    if (MD_OP_FLAGS(rs->op) & F_STORE){
	      sim_num_loads++;
	    }
      }
#endif

# if (! CMU_AGGRESSIVE_CODE_ELIMINATION )
      for (i=0; i<MAX_ODEPS; i++) {
        if (rs->odep_list[i])
	      panic ("retired instruction has odeps\n");
      }
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )
# if 0//(! CMU_AGGRESSIVE_CODE_ELIMINATION )
	  st->sim_slip += (sim_cycle - rs->time_stamp);
	
	  /* indicate to pipeline trace that this instruction retired */
	  ptrace_newstage(rs->ptrace_seq, PST_COMMIT, rs->events);
	  ptrace_endinst(rs->ptrace_seq);
# endif //(! CMU_AGGRESSIVE_CODE_ELIMINATION )


      /* free the instruction data,lsq may use rs after commit,so
       * let it do the job for mem ops
       */
      if (!(st->dcache && (MD_OP_FLAGS(rs->op) & F_MEM))) {
        fetch_return_to_free_list(st,rs);
      }
    
	
	}  /*end of main while loop*/

#ifdef ISTAT
  istat_add_sample(committed);
#endif

  return committed;
}

