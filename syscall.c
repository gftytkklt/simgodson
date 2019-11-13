/*
 * syscall.c - proxy system call handler routines
 *
 * This file is a part of the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *  
 * The tool suite is currently maintained by Doug Burger and Todd M. Austin.
 * 
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
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
 * $Id: syscall.c,v 1.6 2006/11/19 08:08:06 cvsuser Exp $
 *
 * $Log: syscall.c,v $
 * Revision 1.6  2006/11/19 08:08:06  cvsuser
 * Improve the NOC implementation so as to make later-on development more
 * convenient.
 * Unify noc_rr.h and noc_wf.h into noc.h leaving just one "inputbuffer"
 * declaration. WF_Allocation and RR_abitor share almost all noc routines
 * except their corresponding abitration processes in respective file. This
 * is extendible for adding new arbitration feature(though observing from the
 * experiment results, the difference between the two already-exist abitration
 * schemes is trival).
 * Added average latency statistics of each kind of NOC packet.
 *
 * Revision 1.5  2006/10/16 08:27:27  cvsuser
 * improve allocation algor
 *
 * Revision 1.4  2006/10/12 06:43:02  cvsuser
 * originally, stacks of all processes are allocated on the LAST cpu and
 * all heaps reside in the FIRST cpu, which is not reasonable. These
 * modifications help heap and stack allocations happen in local CPU memory.
 * A new "malloc" system call is added to replace the glibc malloc to support
 * the new allocation scheme, because the glibc malloc could not switch malloc
 * starting point in the current simulator.
 *
 * Revision 1.3  2006/10/08 12:24:14  cvsuser
 * change brk syscall
 *
 * Revision 1.2  2006/09/29 05:48:17  cvsuser
 * change brk syscall, and all malloc syscalls are done by sharing one heap space by all processes.
 *
 * Revision 1.1.1.1  2006/09/08 09:21:43  cvsuser
 * Godson-3 simulator
 *
 * Revision 1.3  2005/01/29 09:56:31  fxzhang
 * fix compiler warning
 *
 * Revision 1.2  2005/01/28 10:05:17  fxzhang
 * runnable,without mem hierarchy
 *
 * Revision 1.1.1.1  2004/12/05 14:36:32  fxzhang
 * initial import of ss-mips
 *
 * Revision 1.6  2004/11/19 12:49:50  fxzhang
 * sim outorder
 *
 * Revision 1.5  2004/11/18 04:30:32  fxzhang
 * dump simpoint enhancements
 *
 * Revision 1.4  2004/11/17 05:04:58  fxzhang
 * dump checkpoint support start running
 *
 * Revision 1.3  2004/08/04 08:54:57  fxzhang
 * add aux vector
 * fix mmap support
 *
 * Revision 1.2  2004/08/03 09:50:06  fxzhang
 * merge back changes on AMD64
 *
 * Revision 1.3  2004/07/30 02:48:22  fxzhang
 * fix stat64/lstat64
 *
 * Revision 1.2  2004/07/15 07:31:03  fxzhang
 * fix syscall on x86-64
 *
 * Revision 1.1.1.1  2004/07/15 01:26:34  fxzhang
 * import
 *
 * Revision 1.1.1.1  2004/07/10 16:46:49  fxzhang
 * initial
 *
 * Revision 1.1.1.1  2000/05/26 15:21:54  taustin
 * SimpleScalar Tool Set
 *
 *
 * Revision 1.10  1999/12/13 19:01:07  taustin
 * cross endian execution support added
 *
 * Revision 1.9  1999/03/08 06:41:12  taustin
 * added SVR4 host support
 *
 * Revision 1.8  1998/09/03 22:20:15  taustin
 * added <utime.h> for Linux
 *
 * Revision 1.7  1998/08/31 17:13:29  taustin
 * fixed typo in setrlimit()
 *
 * Revision 1.6  1998/08/27 17:07:37  taustin
 * implemented host interface description in host.h
 * added target interface support
 * moved target-dependent definitions to target files
 * added support for register and memory contexts
 * added support for MS VC++ and CYGWIN32 hosts
 *
 * Revision 1.5  1997/04/16  22:12:17  taustin
 * added Ultrix host support
 *
 * Revision 1.4  1997/03/11  01:37:37  taustin
 * updated copyright
 * long/int tweaks made for ALPHA target support
 * syscall structures are now more MIPS across platforms
 * various target supports added
 *
 * Revision 1.3  1996/12/27  15:56:09  taustin
 * updated comments
 * removed system prototypes
 *
 * Revision 1.1  1996/12/05  18:52:32  taustin
 * Initial revision
 *
 */

 /* This file runs only on linux/x86 now, I am fed up with the kludges
  * for all kinds of platform --fxzhang.
  */

#include <stdio.h>
#include <stdlib.h>

#include "host.h"
#include "misc.h"
#include "mips.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "sim.h"
#include "endian.h"
#include "eio.h"
#include "syscall.h"
#include "mpsyscall.h"

//#define DUMP_SIMPOINT   /* enable simpoint dumping */

/* live execution only support on same-endian hosts... */
#ifdef MD_CROSS_ENDIAN
#error "live execution only support on same-endian hosts!\n"
#endif

#include <unistd.h>
#include <ustat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <linux/utsname.h>		/* struct new_utsname and old_utsname are defined here */
#include <linux/kernel.h>
#include <sys/timex.h>		/* struct timex is defined here */
#include <termios.h>

#include <sys/types.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <sys/statfs.h>
#include <sys/uio.h>
#include <setjmp.h>
#include <sys/times.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <utime.h>
#include <sgtty.h>
#include <sys/dir.h>

/* mips relative data structures are defined in this file */
#include "target-mips/mips_data_structure.h"	
/* linux/mips syscall number and various syscall kernel data structure here */
#include "target-mips/syscalls.h"	


/*--------------------------------------------------------------------*/

/* the following are additions to support multiprocessing */

typedef	struct	_synchqnode
{
	struct	_synchqnode	*next;
	int	pid;
	enum {FREE,
		  WAITING_FOR_LOCK,
		  HOLDING_LOCK,
		  BLOCKED_ON_BARRIER,
		  BLOCKED_ON_SEMAPHORE} synch_state;
	int	synch_var;	/* which synch var for above state */
} SynchQNode;

/* amount by which to increase array sizes for synchronization variables */
#define LOCK_INCREMENT		1024
#define BARRIER_INCREMENT	64
#define SEMAPHORE_INCREMENT	64

/* a (reasonable?) upper bound on number of synch vars of each type */
#define SANITY_LIMIT		65535

/* each processor has a node that may appear in exactly _one_ queue
   associated with a synchronization variable */
SynchQNode	synchq[MAX_CPUS];

/* There is a queue pointer for each lock, barrier, or semaphore,
   and each of these pointers is actually a pointer to the _tail_
   item of the queue, and the 'next' field of the tail item points
   to the head item:

       |___|
       |___|
       |___|--------------------------+
       |___|                          |
       |___|         head             V tail
       |___|        +----+  +----+  +----+
       |___|     +->|next|->|next|->|next|--+
       |___|     |  |    |  |    |  |    |  |
       |___|     |  +----+  +----+  +----+  |
       |___|     |__________________________+
       |   |
     array of 
     pointers
     (one array
      for each
      type of
      synch var)

  The array of pointers may grow in size as more synchronization variables
  of a particular type are needed by the user program.
*/
       
static	SynchQNode	**locks;
static	int	num_allocated_locks = 0;
static	int	num_used_locks = 0;

static	SynchQNode	**barriers;
static	int	*barrier_counts;
static	int	num_allocated_barriers = 0;
static	int	num_used_barriers = 0;

static	SynchQNode	**semaphores;
static	int	*sema_counts;
static	int	num_allocated_semaphores = 0;
static	int	num_used_semaphores = 0;

#define NEW_THREAD_STACK_BASE	0x6ffffff0	/* arbitrary choice */
#define NEW_THREAD_STACK_SIZE	(1 << 20)	/* 1 Megabyte */

#define NEW_THREAD_HEAP_SIZE	(1 << 24)	/* 16 Megabyte */

/* the following is made volatile because it is changed with a new process
   and it is the upper bound of a 'for' loop nested in the main sim loop */
volatile int num_created_processes = 1;

static int num_terminated_processes = 0;

/* initialize architected register state */
void
regs_assign(int pid, md_addr_t stack_top)
{
  unsigned int old_stack_size;
  unsigned int nbytes;
  md_addr_t old_addr, new_addr;

  /* set up initial register state (pid determines how to set) */
  if (pid != 0)
  {
    free(cpus[pid].mem);
    cpus[pid].mem = cpus[0].mem;
    //memcpy(cpus[pid].mem, cpus[0].mem, sizeof(*(cpus[pid].mem)));

    /* FIXME: assuming all entries should be zero... */
    memcpy(&cpus[pid].regs, &cpus[0].regs, sizeof(cpus[pid].regs));

    cpus[pid].ld_stack_base = stack_top;
    old_stack_size = cpus[0].ld_stack_base - cpus[0].regs.regs_R[MD_REG_SP];
    cpus[pid].regs.regs_R[MD_REG_SP] = stack_top - old_stack_size;

    nbytes = old_stack_size;
    old_addr = cpus[0].regs.regs_R[MD_REG_SP];
    new_addr = cpus[pid].regs.regs_R[MD_REG_SP];
    while(nbytes-- > 0){
      MEM_WRITE_BYTE(cpus[pid].mem, new_addr, MEM_READ_BYTE(cpus[0].mem, old_addr));
      old_addr += sizeof(byte_t);
      new_addr += sizeof(byte_t);
    }

    /* get current value of global pointer */
    cpus[pid].regs.regs_R[MD_REG_GP] = cpus[0].regs.regs_R[MD_REG_GP];
    cpus[pid].regs.regs_PC  = cpus[0].regs.regs_PC;
    cpus[pid].fetch_reg_PC  = cpus[0].regs.regs_R[31];

    cpus[pid].ld_text_base  = cpus[0].ld_text_base;
    cpus[pid].ld_text_size  = cpus[0].ld_text_size;
    cpus[pid].ld_data_base  = cpus[0].ld_data_base;
    cpus[pid].ld_data_size  = cpus[0].ld_data_size;
    cpus[pid].ld_prog_entry = cpus[0].ld_prog_entry;

    mem_init1(pid);

  }
}

/*--------------------------------------------------------------------*/

/* This function is called from ss_syscall() below when the user program
   executes the runtime library code for thread creation. The 'wrapper'
   function is also in the runtime library code, and is the function that
   is actually called when the new thread is created. The 'func_ptr'
   refers to the user program function that will then be called from
   the wrapper function in the runtime library. Upon return from the
   user function, the wrapper function will invoke the 'terminate thread'
   system call and thereby support the semantics of the return from the
   user function causing thread termination.
*/
static void	CreateNewProcess ()
{
    int	new_pid;
    md_addr_t stack_top;

    new_pid = num_created_processes++;
    if (num_created_processes > MAX_CPUS)
    {
    	panic ("too many processes; the limit is %d\n", MAX_CPUS);
    }

    /* determine new top of stack;
       this pointer will be passed on to register initialization */
    stack_top = (new_pid+1) * NEW_THREAD_TOTAL_SIZE - 0x20000;

    /* the entry point (PC value) for a new thread is the 'wrapper' function */
    regs_assign(new_pid, stack_top);

	cpus[new_pid].mmap_base = MMAP_START_ADDR + new_pid * NEW_THREAD_TOTAL_SIZE;
	cpus[new_pid].mmap_size = 0;
	
    /* for synch queue node for this thread, set the pid and state fields */
    synchq[new_pid].pid = new_pid;
    synchq[new_pid].synch_state = FREE;	/* not waiting, holding, or blocked */

    /* mark the new thread/process as active */
    cpus[new_pid].active = 1;

    /* because the thread is now marked as active, and the total number
       of created processes has been incremented, the main simulation loop
       will shortly execute the first instruction of the new thread */
}

extern int is_splash;

/* syscall proxy handler, architect registers and memory are assumed to be
   precise when this function is called, register and memory are updated with
   the results of the sustem call */
void
sys_syscall(struct godson2_cpu *st, /* cpu to access */
			struct regs_t *regs,	/* registers to access */
	    	mem_access_fn mem_fn,	/* generic memory accessor */
	    	struct mem_t *mem,		/* memory space to access */
	    	md_inst_t inst,			/* system call inst */
	    	int traceable)			/* traceable system call? */
{
  word_t syscode = regs->regs_R[2];

  int pid = st->cpuid;
  
  //warn("syscall %d,a0=%x,a1=%x,a2=%x\n",syscode,regs->regs_R[4],regs->regs_R[5],regs->regs_R[6]);

  /* first, check if an EIO trace is being consumed... */
  if (traceable && sim_eio_fd != NULL)
    {
      eio_read_trace(sim_eio_fd, st->sim_pop_insn, regs, mem_fn, mem, inst);

      /* fini... */
      return;
    }
#ifdef MD_CROSS_ENDIAN
  else if (syscode == SS_SYS_exit)
    {
      /* exit jumps to the target set in main() */
      longjmp(sim_exit_buf, /* exitcode + fudge */regs->regs_R[4]+1);
    }
  else
    fatal("cannot execute MIPS system call on cross-endian host");

#else /* !MD_CROSS_ENDIAN */

  /* no, OK execute the live system call... */
  switch (syscode)
    {
    case SS_SYS_exit:
      /* exit jumps to the target set in main() */
      longjmp(sim_exit_buf, /* exitcode + fudge */regs->regs_R[4]+1);
      break;

#if 0
	/* simplescalar does not support multithread */
    case SS_SYS_fork:
      /* FIXME: this is broken... */
      regs->regs_R[2] = fork();
      if (regs->regs_R[2] != -1)
	{
	  regs->regs_R[7] = 0;
	  /* parent process */
	  if (regs->regs_R[2] != 0)
	  regs->regs_R[3] = 0;
	}
      else
	fatal("SYS_fork failed");
      break;
#endif

#if 0
    case SS_SYS_vfork:
      /* FIXME: this is broken... */
      int r31_parent = regs->regs_R[31];
      /* pid */regs->regs_R[2] = vfork();
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	fatal("vfork() in SYS_vfork failed");
      if (regs->regs_R[2] != 0)
	{
	  regs->regs_R[3] = 0;
	  regs->regs_R[7] = 0;
	  regs->regs_R[31] = r31_parent;
	}
      break;
#endif

    case SS_SYS_read:
      {
	char *buf;

	/* allocate same-sized input buffer in host memory */
	if (!(buf = (char *)calloc(/*nbytes*/regs->regs_R[6], sizeof(char))))
	  fatal("out of memory in SYS_read");

	/* redirect stdin to support multiple stdin */
	if (st->stdin_addr) 
	  freopen(st->stdin_addr, "r", stdin);
	
	/* read data from file */
	/*nread*/regs->regs_R[2] =
	  read(/*fd*/regs->regs_R[4], buf, /*nbytes*/regs->regs_R[6]);

	/* check for error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* copy results back into host memory */
	mem_bcopy(mem_fn, mem,
		  Write, /*buf*/regs->regs_R[5],
		  buf, /*nread*/regs->regs_R[2]);

	/* done with input buffer */
	free(buf);
      }
      break;


    case SS_SYS_write:
      {
	char *buf;

	/* allocate same-sized output buffer in host memory */
	if (!(buf = (char *)calloc(/*nbytes*/regs->regs_R[6], sizeof(char))))
	  fatal("out of memory in SYS_write");

	/* copy inputs into host memory */
	mem_bcopy(mem_fn, mem,
		  Read, /*buf*/regs->regs_R[5],
		  buf, /*nbytes*/regs->regs_R[6]);

	/* write data to file */
	if (sim_progfd && MD_OUTPUT_SYSCALL(regs))
	  {
	    /* redirect program output to file */

	    /*nwritten*/regs->regs_R[2] =
	      fwrite(buf, 1, /*nbytes*/regs->regs_R[6], sim_progfd);
	  }
	else
	  {
	    /* perform program output request */

	    /*nwritten*/regs->regs_R[2] =
	      write(/*fd*/regs->regs_R[4],
		    buf, /*nbytes*/regs->regs_R[6]);
	  }

	/* check for an error condition */
	if (regs->regs_R[2] == regs->regs_R[6])
	  /*result*/regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* done with output buffer */
	free(buf);
      }
      break;


    case SS_SYS_open:
      {
	char buf[MAXBUFSIZE];
	unsigned int i;
	int ss_flags,ss_flags_save;
	int local_flags = 0;

	ss_flags_save = ss_flags = regs->regs_R[5];

	/* translate open(2) flags */
	for (i=0; i<SS_NFLAGS; i++)
	  {
	    if (ss_flags & ss_flag_table[i].ss_flag)
	      {
		ss_flags &= ~ss_flag_table[i].ss_flag;
		local_flags |= ss_flag_table[i].local_flag;
	      }
	  }
	/* any target flags left? */
	if (ss_flags != 0)
	  warn("syscall: open: cannot decode flags: 0x%08x", ss_flags);

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* open the file */
#ifdef __CYGWIN32__
	/*fd*/regs->regs_R[2] =
	  open(buf, local_flags|O_BINARY, /*mode*/regs->regs_R[6]);
#else /* !__CYGWIN32__ */
	/*fd*/regs->regs_R[2] =
	  open(buf, local_flags, /*mode*/regs->regs_R[6]);
#endif /* __CYGWIN32__ */

         //myfprintf(stderr,"filename %s,flag %x,addr=%x\n",buf,ss_flags_save,regs->regs_R[4]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1) {
	  regs->regs_R[7] = 0;

#ifdef DUMP_SIMPOINT
	  {
	    extern void record_file_open(int fd,int flags,int mode,char *buf);
	    record_file_open(regs->regs_R[2],ss_flags_save,regs->regs_R[6],buf);
	  }
#endif
	} else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


    case SS_SYS_close:
      /* don't close stdin, stdout, or stderr as this messes up sim logs */
      if (/*fd*/regs->regs_R[4] == 0
	  || /*fd*/regs->regs_R[4] == 1
	  || /*fd*/regs->regs_R[4] == 2)
	{
	  regs->regs_R[7] = 0;
	  break;
	}

      /* close the file */
      regs->regs_R[2] = close(/*fd*/regs->regs_R[4]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1) {
	regs->regs_R[7] = 0;
#ifdef DUMP_SIMPOINT
      /* ignore errors */
	{
	  extern void record_file_close(int fd);
	  record_file_close(regs->regs_R[4]);
	}
#endif
      }
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
      break;


#if 0
	/* simplescalar does not support multithread */
    case SS_SYS_waitpid:
    break;
#endif


    case SS_SYS_creat:
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* create the file */
	/*fd*/regs->regs_R[2] = creat(buf, /*mode*/regs->regs_R[5]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1) {
	  regs->regs_R[7] = 0;

#ifdef DUMP_SIMPOINT
	  {
	    extern void record_file_open(int fd,int flags,int mode,char *buf);
	    record_file_open(regs->regs_R[2],SS_O_CREAT|SS_O_WRONLY|SS_O_TRUNC,regs->regs_R[5],buf);
	  }
#endif
	}else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


	case SS_SYS_link:
	{
	  char old_name[MAXBUFSIZE], new_name[MAXBUFSIZE];

	  /* copy old file name and new file name to host memory */
	  mem_strcpy(mem_fn, mem, Read, /*oldname*/regs->regs_R[4], old_name);
	  mem_strcpy(mem_fn, mem, Read, /*newname*/regs->regs_R[5], new_name);

	  /* link files */
	  regs->regs_R[2] = link(old_name, new_name);
	  if (regs->regs_R[2] != -1)
	  	regs->regs_R[7] = 0;
	  else
	  	{
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	  	}
	}
	break;

	
    case SS_SYS_unlink:
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* delete the file */
	/*result*/regs->regs_R[2] = unlink(buf);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


    /* syscall SS_SYS_execve() 4010 is not implemented for simplescalar */  

    
    case SS_SYS_chdir:
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* change the working directory */
	/*result*/regs->regs_R[2] = chdir(buf);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


	/* get the system time */
	case SS_SYS_time:
	  {
	  	time_t * tloc;
	  	tloc = (time_t *)malloc(sizeof(time_t));
	  	if (!tloc)
	  	  fatal("out of virtual memory in SYS_SS_time");
	  	
		/* copy the arguments to the host memory */
	  	mem_bcopy(mem_fn, mem, Read, regs->regs_R[4],
		  tloc, sizeof(time_t));

		/* get the system time */
		regs->regs_R[2] = time(tloc);
		
		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, regs->regs_R[4],
		  tloc, sizeof(time_t));

		/* check for an error condition */
		if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	    /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	    free(tloc);
	  }
	break;


	case SS_SYS_mknod:
	  {
	  	char buf[MAXBUFSIZE];

	  	/* copy filename to host memory */
	    mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	  	/*result*/regs->regs_R[2] = mknod(buf, regs->regs_R[5], regs->regs_R[6]);
		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


    case SS_SYS_chmod:
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* chmod the file */
	/*result*/regs->regs_R[2] = chmod(buf, /*mode*/regs->regs_R[5]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


case SS_SYS_lchown:
#ifdef _MSC_VER
      warn("syscall chown() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* chown the file */
	/*result*/regs->regs_R[2] = lchown(buf, /*owner*/regs->regs_R[5],
				    /*group*/regs->regs_R[6]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
#endif /* _MSC_VER */
      break;


     case SS_SYS_stat:
      {
#if 0 /* mips mark it as unimplemented */
	char buf[MAXBUFSIZE];
	struct ss_old_statbuf ss_old_sbuf;
	struct __old_kernel_stat old_sbuf;
	
	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fName*/regs->regs_R[4], buf);

	/* stat() the file */
	/*result*/regs->regs_R[2] = stat(buf, &old_sbuf);
	
	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate from host stat structure to target format */
	ss_old_sbuf.ss_st_dev = (word_t)MD_SWAPH(old_sbuf.st_dev);
	ss_old_sbuf.ss_st_ino = (word_t)MD_SWAPH(old_sbuf.st_ino);
	ss_old_sbuf.ss_st_mode = (word_t)MD_SWAPH(old_sbuf.st_mode);
	ss_old_sbuf.ss_st_nlink = (word_t)MD_SWAPH(old_sbuf.st_nlink);
	ss_old_sbuf.ss_st_uid = (word_t)MD_SWAPH(old_sbuf.st_uid);
	ss_old_sbuf.ss_st_gid = (word_t)MD_SWAPH(old_sbuf.st_gid);
	ss_old_sbuf.ss_st_rdev = (word_t)MD_SWAPH(old_sbuf.st_rdev);
	ss_old_sbuf.ss_st_size = (long)MD_SWAPW(old_sbuf.st_size);
#if 0
	ss_old_sbuf.ss_st_atime = (word_t)MD_SWAPW(old_sbuf.st_atime);
	ss_old_sbuf.ss_st_mtime = (word_t)MD_SWAPW(old_sbuf.st_mtime);
	ss_old_sbuf.ss_st_ctime = (word_t)MD_SWAPW(old_sbuf.st_ctime);
#endif

	ss_old_sbuf.ss_st_res1 = ss_old_sbuf.ss_st_res2 = ss_old_sbuf.ss_st_res3 = 0;
	ss_old_sbuf.ss_st_blksize = 0;
	ss_old_sbuf.ss_st_blocks = 0;
	ss_old_sbuf.ss_st_unused[0] = 0;
	ss_old_sbuf.ss_st_unused[1] = 0;
	
	/* copy stat() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*old_sbuf*/regs->regs_R[5],
		  &ss_old_sbuf, sizeof(struct ss_old_statbuf));
#else
	/* got an error, return details */
	regs->regs_R[2] = -ENOSYS;
	regs->regs_R[7] = 1;
#endif
      }
      break;

    
    case SS_SYS_lseek:
      /* seek into file */
      regs->regs_R[2] =
	lseek(/*fd*/regs->regs_R[4],
	      /*off*/regs->regs_R[5], /*dir*/regs->regs_R[6]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
      break;

      
    case SS_SYS_getpid:
#ifdef _MSC_VER
      warn("syscall getuid() not yet implemented for MSC...");
#else /*!_MSC_VER*/
      /* get the simulator process id */
      /*result*/regs->regs_R[2] = getpid();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /*_MSC_VER*/
      break;


#if 0
    case SS_SYS_mount:
    {
		char dev_name[MAXBUFSIZE], dir_name[MAXBUFSIZE], type[MAXBUFSIZE];
		unsigned long *data, addr;

		data = (unsigned long *)malloc(sizeof(unsigned long));
		if (!data)
		  fatal ("out of virtual memory in SS_SYS_mount");
		
    	/* copy arguments to the host memory and translate it to the host format */
    	mem_strcpy(mem_fn, mem, Read, /*dev_name*/regs->regs_R[4], dev_name);
        mem_strcpy(mem_fn, mem, Read, /*dir_name*/regs->regs_R[5], dir_name);
        mem_strcpy(mem_fn, mem, Read, /*type*/regs->regs_R[6], type);
        /* locate the argument first and then copy it to the host memory */ 
        mem_bcopy(mem_fn, mem, Read, regs->regs_R[29]+16,
          &addr, sizeof(unsigned long));
        addr = MD_SWAPW(addr);
        mem_bcopy(mem_fn, mem, Read, /*data*/addr, data, sizeof(unsigned long));
		*data = MD_SWAPW(*data);
		
		/*result*/regs->regs_R[2] = mount(dev_name, dir_name, type, 
			regs->regs_R[7], data);
		
		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
        else
	    {
	    /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
        free(data);
    }	
    break;


	case SS_SYS_oldumount:
	{
		char name[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*name*/regs->regs_R[4], name);

		/*result*/regs->regs_R[2] = umount(name, 0);
		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
    break;
#endif


    case SS_SYS_setuid:
#ifdef _MSC_VER_
	warn("syscall setuid() not yet implemented for MSC...");
	regs->regs_R[7] = 0;
#else
	{
		half_t uid;

    	/* copy arguments to the host memory */
    	uid = (half_t)regs->regs_R[4];

    	regs->regs_R[2] = setuid(uid);

    	/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
#endif /*_MSC_VER_*/
    break;


    case SS_SYS_getuid:
#ifdef _MSC_VER
      warn("syscall getuid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      /* get current user id */
      /* result */regs->regs_R[2] = getuid();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /* _MSC_VER */
      break;


    case SS_SYS_stime:
	{
		/*long*/time_t * tptr;

		tptr = (time_t*)malloc(sizeof(time_t));
		if (!tptr)
		  fatal("out of virtual memory in SS_SYS_stime");

		/* copy arguments to the host memory */
		mem_bcopy(mem_fn, mem, Read, regs->regs_R[4],
		  tptr, sizeof(time_t));

		/* translate from host time_t structure to target format */
		*tptr = MD_SWAPW(*tptr);
		
		/*result*/regs->regs_R[2] = stime(tptr);
		
		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	    free(tptr);
    }
	break;


#if 0
	case SS_SYS_ptrace:
	{
		/*result*/regs->regs_R[2] = ptrace(/*request*/regs->regs_R[4], /*pid*/regs->regs_R[5], 
			                              /*addr*/regs->regs_R[6], /*data*/regs->regs_R[7]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;
#endif


	case SS_SYS_alarm:
    {
		/*result*/regs->regs_R[2] = alarm(/*seconds*/regs->regs_R[4]);    

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
    break;


case SS_SYS_fstat:
      {
#if 0 /* linux/mips have not implement this */
	struct ss_old_statbuf ss_old_sbuf;
	struct __old_kernel_stat old_sbuf;

	/* fstat() the file */
	/*result*/regs->regs_R[2] = fstat(/*fd*/regs->regs_R[4], &old_sbuf);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate from host stat structure to target format */
	ss_old_sbuf.ss_st_dev = (word_t)MD_SWAPH(old_sbuf.st_dev);
	ss_old_sbuf.ss_st_ino = (word_t)MD_SWAPH(old_sbuf.st_ino);
	ss_old_sbuf.ss_st_mode = (word_t)MD_SWAPH(old_sbuf.st_mode);
	ss_old_sbuf.ss_st_nlink = (word_t)MD_SWAPH(old_sbuf.st_nlink);
	ss_old_sbuf.ss_st_uid = (word_t)MD_SWAPH(old_sbuf.st_uid);
	ss_old_sbuf.ss_st_gid = (word_t)MD_SWAPH(old_sbuf.st_gid);
	ss_old_sbuf.ss_st_rdev = (word_t)MD_SWAPH(old_sbuf.st_rdev);
	ss_old_sbuf.ss_st_size = (long)MD_SWAPW(old_sbuf.st_size);
#if 0
	ss_old_sbuf.ss_st_atime = (word_t)MD_SWAPW(old_sbuf.st_atime);
	ss_old_sbuf.ss_st_mtime = (word_t)MD_SWAPW(old_sbuf.st_mtime);
	ss_old_sbuf.ss_st_ctime = (word_t)MD_SWAPW(old_sbuf.st_ctime);
#endif

	ss_old_sbuf.ss_st_res1 = ss_old_sbuf.ss_st_res2 = ss_old_sbuf.ss_st_res3 = 0;
	ss_old_sbuf.ss_st_blksize = 0;
	ss_old_sbuf.ss_st_blocks = 0;
	ss_old_sbuf.ss_st_unused[0] = 0;
	ss_old_sbuf.ss_st_unused[1] = 0;

	/* copy fstat() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*old_sbuf*/regs->regs_R[5],
		  &ss_old_sbuf, sizeof(struct ss_old_statbuf));
#else
	      regs->regs_R[2] = -ENOSYS;
	      regs->regs_R[7] = 1;
#endif
      }
      break;


	  case SS_SYS_pause:
	  {
		/*result*/regs->regs_R[2] = pause();

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	  }
	  break;
	  

	case SS_SYS_utime:
	{
		char buf[MAXBUFSIZE];
		struct utimbuf times;
		struct ss_utimbuf ss_times;

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*filename*/regs->regs_R[4], buf);
		mem_bcopy(mem_fn, mem, Read, /*times*/regs->regs_R[5],
			&ss_times, sizeof(struct ss_utimbuf));

		/* translate from target utimbuf structure to host format */
		times.actime = MD_SWAPW(ss_times.ss_actime);
		times.modtime = MD_SWAPW(ss_times.ss_modtime);
	
		/*result*/regs->regs_R[2] = utime(buf, &times);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_access:
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fName*/regs->regs_R[4], buf);

	/* check access on the file */
	/*result*/regs->regs_R[2] = access(buf, /*mode*/regs->regs_R[5]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


	/* this syscall may be not used */
	case SS_SYS_nice:
	{
		/*result*/regs->regs_R[2] = nice(regs->regs_R[4]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_sync:
	{
		/* no return value */
		sync();

		/* check for an error condition *
		 * may be not used          */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


    /* this syscall may be not used, the simplescalar does not support multithread */
    case SS_SYS_kill:
	{
		/*result*/regs->regs_R[2] = kill(/*pid*/regs->regs_R[4], /*sig*/regs->regs_R[5]);	

		/* check for an error condition */
		if (regs->regs_R[2] != -1)
		  regs->regs_R[7] =0;
		else
		{
			regs->regs_R[2] = errno;
			regs->regs_R[7] = 1;
		}
	}
	break;


	case SS_SYS_rename:
	{
		char oldname[MAXBUFSIZE], newname[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*oldname*/regs->regs_R[4], oldname);
		mem_strcpy(mem_fn, mem, Read, /*newname*/regs->regs_R[5], newname);

		/*result*/regs->regs_R[2] = rename(oldname, newname);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_mkdir:
	{
		char buf[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*pathname*/regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = mkdir(buf, /*mode*/regs->regs_R[5]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_rmdir:
	{
		char buf[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*pathname*/regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = rmdir(buf);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	  case SS_SYS_dup:
      /* dup() the file descriptor */
      /*fd*/regs->regs_R[2] = dup(/*fd*/regs->regs_R[4]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
      break;


#ifndef _MSC_VER
    case SS_SYS_pipe:
      {
	int fd[2];

	/* copy pipe descriptors to host memory */;
	mem_bcopy(mem_fn, mem, Read, /*fd's*/regs->regs_R[4], fd, sizeof(fd));

	/* create a pipe */
	/*result*/regs->regs_R[7] = pipe(fd);

	/* copy descriptor results to result registers */
	/*pipe1*/regs->regs_R[2] = fd[0];
	/*pipe2*/regs->regs_R[3] = fd[1];

	/* check for an error condition */
	if (regs->regs_R[7] == -1)
	  {
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
    break;
#endif
	

	case SS_SYS_times:
	{
		struct tms tbuf;
		struct ss_tms ss_tbuf;

		/*result*/regs->regs_R[2] = times(&tbuf);

		/* translate from host tms structure to target format */
		ss_tbuf.ss_tms_utime = MD_SWAPW(tbuf.tms_utime);
		ss_tbuf.ss_tms_stime = MD_SWAPW(tbuf.tms_stime);
		ss_tbuf.ss_tms_cutime = MD_SWAPW(tbuf.tms_cutime);
		ss_tbuf.ss_tms_cstime = MD_SWAPW(tbuf.tms_cstime);

		/* copy result to the target memory */
		mem_bcopy(mem_fn, mem, Write, regs->regs_R[4],
		  &ss_tbuf, sizeof(struct ss_tms));
		
		/* check for an error condition */
		if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
		else
		{
		  regs->regs_R[7] = 1;
		  regs->regs_R[2] = errno;
		}
	}
	break;


	/* this syscall can only run on the mips platform, since the return value
	   is different from the very syscall of other platform discribed in the document */
    case SS_SYS_brk:
	{
	  md_addr_t addr;

	  /* round the new heap pointer to the its page boundary */
	  //addr = ROUND_UP(/*base*/regs->regs_R[4], MD_PAGE_SIZE);
	  addr = regs->regs_R[4];

	  /* check whether heap area has merged with stack area */
	  if (addr >= cpus[0].ld_brk_point && addr < cpus[0].regs.regs_R[29])
	  {
		regs->regs_R[2] = addr;
		regs->regs_R[7] = 0;
		cpus[0].ld_brk_point = addr;
		printf("ld_brk_point addr is 0x%08x, cpuid=%d\n",cpus[0].ld_brk_point, st->cpuid);
	  }
	  else if (addr >= cpus[0].regs.regs_R[29])
	  {
		printf("overflow:sp=0x%08x, addr=0x%08x, cpuid=%d\n",cpus[0].regs.regs_R[29], addr, st->cpuid);
		/* out of address space, indicate error */
		regs->regs_R[2] = ENOMEM;
		regs->regs_R[7] = 1;
	  }
	  else
	  {
		printf("stack-shrink:ld_brk_point=0x%08x, addr=0x%08x, cpuid=%d\n",cpus[0].ld_brk_point, addr, st->cpuid);
		regs->regs_R[2] = cpus[0].ld_brk_point;
		regs->regs_R[7] = 0;
	  }

	}
	break;	


	case SS_SYS_setgid:
#ifdef _MSC_VER_
	warn("syscall setgid() not yet implemented for MSC...");
	regs->regs_R[7] = 0;
#else
		/*result*/regs->regs_R[2] = setgid(/*gid*/regs->regs_R[4]);

		/* check for an error condition */
      	if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
      	else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
#endif /*_MSC_VER_*/
	break;


	 case SS_SYS_getgid:
#ifdef _MSC_VER
      warn("syscall getgid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      /* get current group id */
      /* result */regs->regs_R[2] = getgid();

	/* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /* _MSC_VER */
      break;


	case SS_SYS_geteuid:
#ifdef _MSC_VER
      warn("syscall geteuid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      /* get effective user id */
      /* result */regs->regs_R[2] = geteuid();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /* _MSC_VER */
      break;


	case SS_SYS_getegid:
#ifdef _MSC_VER
      warn("syscall getgid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      /* get current effective group id */
      /* result */regs->regs_R[2] = getegid();

	/* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /* _MSC_VER */
      break;   


	case SS_SYS_acct:
	{
		char buf[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = acct(buf);

		/* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


#if 0
    case SS_SYS_umount:
    {
    	char name[MAXBUFSIZE];

    	/* copy arguments to the host memory */
    	mem_strcpy(mem_fn, mem, Read, /*name*/regs->regs_R[4], name);

    	/*result*/regs->regs_R[2] = umount(name, /*flags*/regs->regs_R[5]);

    	/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
    }
    break;
#endif


 case SS_SYS_ioctl:
      {
	char buf[NUM_IOCTL_BYTES];
	int local_req = 0;

	/* convert target ioctl() request to host ioctl() request values */
	switch (/*req*/regs->regs_R[5]) {
#if 0
#ifdef TIOCGETP
	case SS_IOCTL_TIOCGETP:
	  local_req = TIOCGETP;
	  break;
#endif
#ifdef TIOCSETP
	case SS_IOCTL_TIOCSETP:
	  local_req = TIOCSETP;
	  break;
#endif
#ifdef TIOCGETP
	case SS_IOCTL_TCGETP:
	  local_req = TIOCGETP;
	  break;
#endif
#ifdef TCGETA
	case SS_IOCTL_TCGETA:
	  local_req = TCGETA;
	  break;
#endif
#ifdef TIOCGLTC
	case SS_IOCTL_TIOCGLTC:
	  local_req = TIOCGLTC;
	  break;
#endif
#ifdef TIOCSLTC
	case SS_IOCTL_TIOCSLTC:
	  local_req = TIOCSLTC;
	  break;
#endif
#ifdef TIOCGWINSZ
	case SS_IOCTL_TIOCGWINSZ:
	  local_req = TIOCGWINSZ;
	  break;
#endif
#ifdef TCSETAW
	case SS_IOCTL_TCSETAW:
	  local_req = TCSETAW;
	  break;
#endif
#ifdef TIOCGETC
	case SS_IOCTL_TIOCGETC:
	  local_req = TIOCGETC;
	  break;
#endif
#ifdef TIOCSETC
	case SS_IOCTL_TIOCSETC:
	  local_req = TIOCSETC;
	  break;
#endif
#ifdef TIOCLBIC
	case SS_IOCTL_TIOCLBIC:
	  local_req = TIOCLBIC;
	  break;
#endif
#ifdef TIOCLBIS
	case SS_IOCTL_TIOCLBIS:
	  local_req = TIOCLBIS;
	  break;
#endif
#ifdef TIOCLGET
	case SS_IOCTL_TIOCLGET:
	  local_req = TIOCLGET;
	  break;
#endif
#ifdef TIOCLSET
	case SS_IOCTL_TIOCLSET:
	  local_req = TIOCLSET;
	  break;
#endif
#else
	  /* linux-mips to x86 */
 case SS_IOCTL_TCGETA      : local_req=TCGETA; break;
 case SS_IOCTL_TCSETA      : local_req=TCSETA     ; break;
 case SS_IOCTL_TCSETAW     : local_req=TCSETAW    ; break;
 case SS_IOCTL_TCSETAF     : local_req=TCSETAF    ; break;
 case SS_IOCTL_TCSBRK      : local_req=TCSBRK     ; break;
 case SS_IOCTL_TCXONC      : local_req=TCXONC     ; break;
 case SS_IOCTL_TCFLSH      : local_req=TCFLSH     ; break;
 case SS_IOCTL_TCGETS      : local_req=TCGETS     ; break;
 case SS_IOCTL_TCSETS      : local_req=TCSETS     ; break;
 case SS_IOCTL_TCSETSW     : local_req=TCSETSW    ; break;
 case SS_IOCTL_TCSETSF     : local_req=TCSETSF    ; break;
 case SS_IOCTL_TIOCEXCL    : local_req=TIOCEXCL   ; break;
 case SS_IOCTL_TIOCNXCL    : local_req=TIOCNXCL   ; break;
 case SS_IOCTL_TIOCOUTQ    : local_req=TIOCOUTQ   ; break;
 case SS_IOCTL_TIOCSTI     : local_req=TIOCSTI    ; break;
 case SS_IOCTL_TIOCMGET    : local_req=TIOCMGET   ; break;
 case SS_IOCTL_TIOCMBIS    : local_req=TIOCMBIS   ; break;
 case SS_IOCTL_TIOCMBIC    : local_req=TIOCMBIC   ; break;
 /*
 case SS_IOCTL_TIOCGLTC    : local_req=TIOCGLTC   ; break;
 case SS_IOCTL_TIOCSLTC    : local_req=TIOCSLTC   ; break;
 */
#endif
	}

#if !defined(TIOCGETP) && defined(linux)
        if (!local_req && /*req*/regs->regs_R[5] == SS_IOCTL_TIOCGETP)
          {
            struct termios lbuf;
            struct ss_sgttyb buf;

            /* result */regs->regs_R[2] =
                          tcgetattr(/* fd */(int)regs->regs_R[4], &lbuf);

            /* translate results */
            buf.sg_ispeed = lbuf.c_ispeed;
            buf.sg_ospeed = lbuf.c_ospeed;
            buf.sg_erase = lbuf.c_cc[VERASE];
            buf.sg_kill = lbuf.c_cc[VKILL];
            buf.sg_flags = 0;   /* FIXME: this is wrong... */

            mem_bcopy(mem_fn, mem, Write,
                      /* buf */regs->regs_R[6], &buf,
                      sizeof(struct ss_sgttyb));

            if (regs->regs_R[2] != -1)
              regs->regs_R[7] = 0;
            else /* probably not a typewriter, return details */
              {
                regs->regs_R[2] = errno;
                regs->regs_R[7] = 1;
              }
          }
        else
#endif

	if (!local_req)
	  {
	    /* FIXME: could not translate the ioctl() request, just warn user
	       and ignore the request */
	    warn("syscall: ioctl: ioctl code not supported d=%d, req=%d",
		regs->regs_R[4], regs->regs_R[5]);
	    regs->regs_R[2] = 0;
	    regs->regs_R[7] = 0;
	  }
	else
	  {
#ifdef _MSC_VER
	    warn("syscall getgid() not yet implemented for MSC...");
	    regs->regs_R[7] = 0;
	    break;
#else /* !_MSC_VER */

#if 0 /* FIXME: needed? */
#ifdef TIOCGETP
	    if (local_req == TIOCGETP && sim_progfd)
	      {
		/* program I/O has been redirected to file, make
		   termios() calls fail... */

		/* got an error, return details */
		regs->regs_R[2] = ENOTTY;
		regs->regs_R[7] = 1;
		break;
	      }
#endif
#endif
	    /* ioctl() code was successfully translated to a host code */

	    /* if arg ptr exists, copy NUM_IOCTL_BYTES bytes to host mem */
	    if (/*argp*/regs->regs_R[6] != 0)
	      mem_bcopy(mem_fn, mem,
			Read, /*argp*/regs->regs_R[6], buf, NUM_IOCTL_BYTES);

	    /* perform the ioctl() call */
	    /*result*/regs->regs_R[2] =
	      ioctl(/*fd*/regs->regs_R[4], local_req, buf);

	    /* if arg ptr exists, copy NUM_IOCTL_BYTES bytes from host mem */
	    if (/*argp*/regs->regs_R[6] != 0)
	      mem_bcopy(mem_fn, mem, Write, regs->regs_R[6],
			buf, NUM_IOCTL_BYTES);

	    /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	      {	
		/* got an error, return details */
		regs->regs_R[2] = errno;
		regs->regs_R[7] = 1;
	      }
#endif /* _MSC_VER */
	  }
      }
      break;


	case SS_SYS_fcntl64:/* the fcntl64 syscall may be buggy, so use the fcntl as well */ 
	case SS_SYS_fcntl:
#ifdef _MSC_VER
      warn("syscall fcntl() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      /* get fcntl() information on the file */
      regs->regs_R[2] =
	fcntl(/*fd*/regs->regs_R[4], /*cmd*/regs->regs_R[5],
	      /*arg*/regs->regs_R[6]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /* _MSC_VER */
      break;


	case SS_SYS_setpgid:
#ifdef _MSC_VER_	
	warn("syscall setpgid() not yet implemented for MSC...");
	regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*setpgid*/regs->regs_R[2] = setpgid(/*pid*/regs->regs_R[4], 
	                                     /*pgid*/regs->regs_R[5]);
	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
		regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /*_MSC_VER_*/
    break;
	

	case SS_SYS_olduname:
	{
		struct oldold_utsname *name;
		
		name = (struct oldold_utsname *)malloc(sizeof(struct oldold_utsname));
		if (!name)
	      fatal("out of virtual memory in SYS_uname");
		
		/*result*/regs->regs_R[2] = uname(name);

		/* copy host side memory into target side pointer data */
		mem_bcopy(mem_fn, mem, Write, regs->regs_R[4],
		  name, sizeof(struct oldold_utsname));

		if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
		else
		  {
			regs->regs_R[2] = errno;
		    regs->regs_R[7] = 1;
		  }
 
		/* free the host memory */
		free(name);
	}
	break;


	case SS_SYS_umask:
	{
	  /*result*/regs->regs_R[2] = umask(/*mask*/regs->regs_R[4]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
		regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;
	

	case SS_SYS_chroot:
	{
		char buf[MAXBUFSIZE];

		/* copy arguments to the host memroy */
		mem_strcpy(mem_fn, mem, Read, /*filename*/regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = chroot(buf);

		/* check for an error condition */
      if (regs->regs_R[2] != -1)
		regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


	case SS_SYS_ustat:
	{
		int i;
		struct ustat ubuf;
		struct ss_ustat ss_ubuf;

		/*result*/regs->regs_R[2] = ustat(/*dev*/regs->regs_R[4], &ubuf);

		/* translate from host ustat structure to target format */
		ss_ubuf.ss_f_tfree = MD_SWAPW((long)ubuf.f_tfree);
		ss_ubuf.ss_f_tinode = MD_SWAPW((unsigned long)ubuf.f_tinode);
		for (i=0; i<6; i++)
		{
		  ss_ubuf.ss_f_fname[i] = ubuf.f_fname[i];
		  ss_ubuf.ss_f_fpack[i] = ubuf.f_fpack[i];
		}

		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*ubuf*/regs->regs_R[5], 
		  &ss_ubuf, sizeof(struct ss_ustat));

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_dup2:
      /* dup2() the file descriptor */
      regs->regs_R[2] =
	dup2(/* fd1 */regs->regs_R[4], /* fd2 */regs->regs_R[5]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1) {
	regs->regs_R[7] = 0;
#ifdef DUMP_SIMPOINT
	  {
	    extern void record_file_dup2(int oldfd,int newfd);
	    record_file_dup2(regs->regs_R[4],regs->regs_R[5]);
	  }
#endif
      }
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
      break;


	case SS_SYS_getppid:
#ifdef _MSC_VER
      warn("syscall getppid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*result*/regs->regs_R[2] = getppid();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
#endif /* _MSC_VER */
      break;   


	case SS_SYS_getpgrp:
#ifdef _MSC_VER
      warn("syscall getpgrp() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*result*/regs->regs_R[2] = getpgrp();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
#endif /* _MSC_VER */
      break;    


	case SS_SYS_setsid:
#ifdef _MSC_VER
      warn("syscall setsid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*result*/regs->regs_R[2] = setsid();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
#endif /* _MSC_VER */
      break;    


	/* this syscall may be buggy */
	case SS_SYS_sigaction:
	{
		struct sigaction * act, * oact;

		act = (struct sigaction*)malloc(sizeof(struct sigaction));
		oact = (struct sigaction*)malloc(sizeof(struct sigaction));
		if ((!act)||(!oact))
		  fatal("out of virtual memoyt in SYS_sigaction");

		/* copy arguments to the host memory */
		mem_bcopy(mem_fn, mem, Read, /*act*/regs->regs_R[5],
		  act, sizeof(struct sigaction));

		/*result*/regs->regs_R[2] = sigaction(/*sig*/regs->regs_R[4], act, oact);

		/* copy result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*oact*/regs->regs_R[6], 
		  oact, sizeof(struct sigaction));

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	  free(act);
	  free(oact);
	}
	break;

#if 0
	case SS_SYS_sgetmask:
	{
		/*result*/regs->regs_R[2] = sgetmask();

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_ssetmask:
	{
		/*result*/regs->regs_R[2] = ssetmask(/*newmask*/regs->regs_R[4]);

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;
#endif

	case SS_SYS_setreuid:
	{
		/*result*/regs->regs_R[2] = setreuid(/*ruid*/regs->regs_R[4], 
											/*euid*/regs->regs_R[5]);

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_setregid:
	{
		/*result*/regs->regs_R[2] = setregid(/*ruid*/regs->regs_R[4], 
											/*euid*/regs->regs_R[5]);

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


/* syscall SS_SYS_sigsuspend() 4072 is not implemented for simplescalar */  


	case SS_SYS_sigpending:
	{
	  mips_old_sigset_t ss_buf;
	  sigset_t buf;
	  int i;

	  /*result*/regs->regs_R[2] = sigpending(&buf);

	  ss_buf = 0;
	  for (i=0;i<32;i++) {
	    if (sigismember(&buf,i)) {
	      ss_buf |= (1<<i);
	    }
	  }

	  /* copy result to the target memory */
	  mem_bcopy(mem_fn, mem, Write, /*buf*/regs->regs_R[4], 
	      &ss_buf, sizeof(mips_old_sigset_t));

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


	case SS_SYS_sethostname:
	{
		char buf[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*name*/regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = sethostname(buf, /*len*/regs->regs_R[5]);

		/* check for an error condition */
        if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
        else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }		
	}
	break;


	case SS_SYS_getrlimit:
    case SS_SYS_setrlimit:
#ifdef _MSC_VER
      warn("syscall get/setrlimit() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#elif defined(__CYGWIN32__)
      warn("syscall: called get/setrlimit()\n");
      regs->regs_R[7] = 0;
#else
      {
	/* FIXME: check this..., was: struct rlimit ss_rl; */
	struct ss_rlimit ss_rl;
	struct rlimit rl;

	/* copy rlimit structure to host memory */
	mem_bcopy(mem_fn, mem, Read, /*rlimit*/regs->regs_R[5],
		  &ss_rl, sizeof(struct ss_rlimit));

	/* convert rlimit structure to host format */
	rl.rlim_cur = MD_SWAPW(ss_rl.ss_rlim_cur);
	rl.rlim_max = MD_SWAPW(ss_rl.ss_rlim_max);

	/* get rlimit information */
	if (syscode == SS_SYS_getrlimit)
	  /*result*/regs->regs_R[2] = getrlimit(regs->regs_R[4], &rl);
	else /* syscode == SS_SYS_setrlimit */
	  /*result*/regs->regs_R[2] = setrlimit(regs->regs_R[4], &rl);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate results */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* convert rlimit structure to target format */
	ss_rl.ss_rlim_cur = MD_SWAPW(rl.rlim_cur);
	ss_rl.ss_rlim_max = MD_SWAPW(rl.rlim_max);

	/* copy rlimit structure to target memory */
	mem_bcopy(mem_fn, mem, Write, /*rlimit*/regs->regs_R[5],
		  &ss_rl, sizeof(struct ss_rlimit));
      }
#endif
      break;


	   case SS_SYS_getrusage:
#if defined(__svr4__) || defined(__USLC__) || defined(hpux) || defined(__hpux) || defined(_AIX)
      {
	struct tms tms_buf;
	struct ss_rusage rusage;

	/* get user and system times */
	if (times(&tms_buf) != -1)
	  {
	    /* no error */
	    regs->regs_R[2] = 0;
	    regs->regs_R[7] = 0;
	  }
	else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* initialize target rusage result structure */
#if defined(__svr4__)
	memset(&rusage, '\0', sizeof(struct ss_rusage));
#else /* !defined(__svr4__) */
	bzero(&rusage, sizeof(struct ss_rusage));
#endif

	/* convert from host rusage structure to target format */
	if (mips_cpu_freq > 0) {
	  rusage.ss_ru_utime.ss_tv_sec =
	    ((double)sim_cycle/(double)mips_cpu_freq);
	  rusage.ss_ru_utime.ss_tv_usec =
	    ((double)sim_cycle/(double)mips_cpu_freq)*1000000UL;
	  rusage.ss_ru_stime.ss_tv_sec = 0;
	  rusage.ss_ru_stime.ss_tv_usec = 0;
	} else {
	  rusage.ss_ru_utime.ss_tv_sec = tms_buf.tms_utime/CLK_TCK;
	  rusage.ss_ru_utime.ss_tv_sec = MD_SWAPW(rusage.ss_ru_utime.ss_tv_sec);
	  rusage.ss_ru_utime.ss_tv_usec = 0;
	  rusage.ss_ru_stime.ss_tv_sec = tms_buf.tms_stime/CLK_TCK;
	  rusage.ss_ru_stime.ss_tv_sec = MD_SWAPW(rusage.ss_ru_stime.ss_tv_sec);
	  rusage.ss_ru_stime.ss_tv_usec = 0;
	}

	/* copy rusage results into target memory */
	mem_bcopy(mem_fn, mem, Write, /*rusage*/regs->regs_R[5],
		  &rusage, sizeof(struct ss_rusage));
      }
#elif defined(__unix__) || defined(unix)
      {
	struct rusage local_rusage;
	struct ss_rusage rusage;

	/* get rusage information */
	/*result*/regs->regs_R[2] =
	  getrusage(/*who*/regs->regs_R[4], &local_rusage);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* convert from host rusage structure to target format */
	if (mips_cpu_freq > 0) {
	  rusage.ss_ru_utime.ss_tv_sec =
	    ((double)sim_cycle/(double)mips_cpu_freq);
	  rusage.ss_ru_utime.ss_tv_usec =
	    ((double)sim_cycle/(double)mips_cpu_freq)*1000000UL;
	  rusage.ss_ru_stime.ss_tv_sec = 0;
	  rusage.ss_ru_stime.ss_tv_usec = 0;
	} else {
	  rusage.ss_ru_utime.ss_tv_sec = local_rusage.ru_utime.tv_sec;
	  rusage.ss_ru_utime.ss_tv_usec = local_rusage.ru_utime.tv_usec;
	  rusage.ss_ru_utime.ss_tv_sec = MD_SWAPW(local_rusage.ru_utime.tv_sec);
	  rusage.ss_ru_utime.ss_tv_usec =
	    MD_SWAPW(local_rusage.ru_utime.tv_usec);
	  rusage.ss_ru_stime.ss_tv_sec = local_rusage.ru_stime.tv_sec;
	  rusage.ss_ru_stime.ss_tv_usec = local_rusage.ru_stime.tv_usec;
	  rusage.ss_ru_stime.ss_tv_sec =
	    MD_SWAPW(local_rusage.ru_stime.tv_sec);
	  rusage.ss_ru_stime.ss_tv_usec =
	    MD_SWAPW(local_rusage.ru_stime.tv_usec);
	  rusage.ss_ru_maxrss = MD_SWAPW(local_rusage.ru_maxrss);
	  rusage.ss_ru_ixrss = MD_SWAPW(local_rusage.ru_ixrss);
	  rusage.ss_ru_idrss = MD_SWAPW(local_rusage.ru_idrss);
	  rusage.ss_ru_isrss = MD_SWAPW(local_rusage.ru_isrss);
	  rusage.ss_ru_minflt = MD_SWAPW(local_rusage.ru_minflt);
	  rusage.ss_ru_majflt = MD_SWAPW(local_rusage.ru_majflt);
	  rusage.ss_ru_nswap = MD_SWAPW(local_rusage.ru_nswap);
	  rusage.ss_ru_inblock = MD_SWAPW(local_rusage.ru_inblock);
	  rusage.ss_ru_oublock = MD_SWAPW(local_rusage.ru_oublock);
	  rusage.ss_ru_msgsnd = MD_SWAPW(local_rusage.ru_msgsnd);
	  rusage.ss_ru_msgrcv = MD_SWAPW(local_rusage.ru_msgrcv);
	  rusage.ss_ru_nsignals = MD_SWAPW(local_rusage.ru_nsignals);
	  rusage.ss_ru_nvcsw = MD_SWAPW(local_rusage.ru_nvcsw);
	  rusage.ss_ru_nivcsw = MD_SWAPW(local_rusage.ru_nivcsw);
	}

	/* copy rusage results into target memory */
	mem_bcopy(mem_fn, mem, Write, /*rusage*/regs->regs_R[5],
		  &rusage, sizeof(struct ss_rusage));
      }
#elif defined(__CYGWIN32__) || defined(_MSC_VER)
	    warn("syscall: called getrusage()\n");
            regs->regs_R[7] = 0;
#else
#error No getrusage() implementation!
#endif
      break;

	case SS_SYS_settimeofday:
    case SS_SYS_gettimeofday:
#ifdef _MSC_VER
      warn("syscall gettimeofday() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* _MSC_VER */
      {
	struct ss_timeval ss_tv;
	struct timeval tv, *tvp;
	struct ss_timezone ss_tz;
	struct timezone tz, *tzp;

	if (/*timeval*/regs->regs_R[4] != 0)
	  {
	    /* copy timeval into host memory */
	    mem_bcopy(mem_fn, mem, Read, /*timeval*/regs->regs_R[4],
		      &ss_tv, sizeof(struct ss_timeval));

	    /* convert target timeval structure to host format */
	    tv.tv_sec = MD_SWAPW(ss_tv.ss_tv_sec);
	    tv.tv_usec = MD_SWAPW(ss_tv.ss_tv_usec);

	    tvp = &tv;
	  }
	else
	  tvp = NULL;

	if (/*timezone*/regs->regs_R[5] != 0)
	  {
	    /* copy timezone into host memory */
	    mem_bcopy(mem_fn, mem, Read, /*timezone*/regs->regs_R[5],
		      &ss_tz, sizeof(struct ss_timezone));

	    /* convert target timezone structure to host format */
	    tz.tz_minuteswest = MD_SWAPW(ss_tz.ss_tz_minuteswest);
	    tz.tz_dsttime = MD_SWAPW(ss_tz.ss_tz_dsttime);
	    tzp = &tz;
	  }
	else
	  tzp = NULL;

	if (syscode == SS_SYS_gettimeofday)
	/* get time of day */
	/*result*/regs->regs_R[2] = gettimeofday(tvp, tzp);
	else
	/* set time of day */
	/*result*/regs->regs_R[2] = settimeofday(tvp, tzp);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	if (/*timeval*/regs->regs_R[4] != 0)
	  {
	    /* convert host timeval structure to target format */
	    if (mips_cpu_freq > 0) {
	      ss_tv.ss_tv_sec = (double)sim_cycle/(double)mips_cpu_freq;
	      ss_tv.ss_tv_usec =
		((double)(sim_cycle)/(double)(mips_cpu_freq))*1000000UL;
	    }else{
	      ss_tv.ss_tv_sec = MD_SWAPW(tv.tv_sec);
	      ss_tv.ss_tv_usec = MD_SWAPW(tv.tv_usec);
	    }

	    /* copy timeval to target memory */
	    mem_bcopy(mem_fn, mem, Write, /*timeval*/regs->regs_R[4],
		      &ss_tv, sizeof(struct ss_timeval));
	  }

	if (/*timezone*/regs->regs_R[5] != 0)
	  {
	    /* convert host timezone structure to target format */
	    ss_tz.ss_tz_minuteswest = MD_SWAPW(tz.tz_minuteswest);
	    ss_tz.ss_tz_dsttime = MD_SWAPW(tz.tz_dsttime);

	    /* copy timezone to target memory */
	    mem_bcopy(mem_fn, mem, Write, /*timezone*/regs->regs_R[5],
		      &ss_tz, sizeof(struct ss_timezone));
	  }
      }
#endif /* !_MSC_VER */
      break;


	case SS_SYS_getgroups:
	case SS_SYS_setgroups:
	{
		sword_t * list, size, i;

		size = regs->regs_R[4];
		list = (sword_t*)malloc(size*sizeof(sword_t));
		if (!list)
		fatal("out of virtual memory in SS_SYS_getgroups or SS_SYS_setgroups");	

		/* copy arguments to the host memory */
		mem_bcopy(mem_fn, mem, Read, /*grouplist*/regs->regs_R[5],
		  list, size*sizeof(sword_t));

		/* translate to host format */
		for (i =0; i<size; i++)
		  list[i] = MD_SWAPW((word_t)list[i]);
		
		if (syscode == SS_SYS_getgroups)
		  /*result*/regs->regs_R[2] = getgroups(size, list);
		else/*syscode == SS_SYS_setgroups*/
		  /*result*/regs->regs_R[2] = setgroups(size, list);

		/* translate to host format */
		for (i =0; i<size; i++)
		  list[i] = MD_SWAPW((sword_t)list[i]);

		/* copy arguments to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*grouplist*/regs->regs_R[5],
		  list, size*sizeof(sword_t));

		/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	free(list);
	}
	break;


	case SS_SYS_symlink:
	{
		char oldname[MAXBUFSIZE], newname[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*oldname*/regs->regs_R[4], oldname);
		mem_strcpy(mem_fn, mem, Read, /*newname*/regs->regs_R[5], newname);

		/*result*/regs->regs_R[2] = symlink(oldname, newname);

		/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


     case SS_SYS_lstat:
      {
#if 0
	char buf[MAXBUFSIZE];
	struct ss_old_statbuf ss_old_sbuf;
	struct __old_kernel_stat old_sbuf;

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fName*/regs->regs_R[4], buf);

#ifdef _MSC_VER
	    warn("syscall lstat() not yet implemented for MSC...");
	    regs->regs_R[7] = 0;
	    break;
#else /* !_MSC_VER */
	    /*result*/regs->regs_R[2] = lstat(buf, &old_sbuf);
#endif /* _MSC_VER */
	  

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate from host stat structure to target format */
	ss_old_sbuf.ss_st_dev = (word_t)MD_SWAPH(old_sbuf.st_dev);
	ss_old_sbuf.ss_st_ino = (word_t)MD_SWAPH(old_sbuf.st_ino);
	ss_old_sbuf.ss_st_mode = (word_t)MD_SWAPH(old_sbuf.st_mode);
	ss_old_sbuf.ss_st_nlink = (word_t)MD_SWAPH(old_sbuf.st_nlink);
	ss_old_sbuf.ss_st_uid = (word_t)MD_SWAPH(old_sbuf.st_uid);
	ss_old_sbuf.ss_st_gid = (word_t)MD_SWAPH(old_sbuf.st_gid);
	ss_old_sbuf.ss_st_rdev = (word_t)MD_SWAPH(old_sbuf.st_rdev);
	ss_old_sbuf.ss_st_size = (long)MD_SWAPW(old_sbuf.st_size);
#if 0
	ss_old_sbuf.ss_st_atime = (word_t)MD_SWAPW(old_sbuf.st_atime);
	ss_old_sbuf.ss_st_mtime = (word_t)MD_SWAPW(old_sbuf.st_mtime);
	ss_old_sbuf.ss_st_ctime = (word_t)MD_SWAPW(old_sbuf.st_ctime);
#endif

	ss_old_sbuf.ss_st_res1 = ss_old_sbuf.ss_st_res2 = ss_old_sbuf.ss_st_res3 = 0;
	ss_old_sbuf.ss_st_blksize = 0;
	ss_old_sbuf.ss_st_blocks = 0;
	ss_old_sbuf.ss_st_unused[0] = 0;
	ss_old_sbuf.ss_st_unused[1] = 0;

	/* copy stat() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*old_sbuf*/regs->regs_R[5],
		  &ss_old_sbuf, sizeof(struct ss_old_statbuf));
#else	   
	      regs->regs_R[2] = -ENOSYS;
	      regs->regs_R[7] = 1;
#endif
      }
      break;
      

	case SS_SYS_readlink:
	{
		char path[MAXBUFSIZE], buf[MAXBUFSIZE];
		int bufsize = regs->regs_R[6];

		/* copy the arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*path*/regs->regs_R[4], path);

		/*result*/regs->regs_R[2] = readlink(path, buf, bufsize);

		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*buf*/regs->regs_R[5], 
		  buf, bufsize*sizeof(char));

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	     regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, indicate result */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }	
	}
	break;


	case SS_SYS_uselib:
	{
		char buf[MAXBUFSIZE];

		/* copy the arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*libary*/regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = uselib(buf);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, indicate result */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;
	

#if 0
	case SS_SYS_swapon:
	{
		char buf[MAXBUFSIZE];

		/* copy the arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*specialfile*/regs->regs_R[4], buf);

		/* this line may be buggy */
		/*result*/regs->regs_R[2] = swapon(buf, /*swap_flags*/regs->regs_R[5]);

		/* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;
#endif


/* syscall reboot() 4088 is not implemented for simplescalar */

/* syscall old_readdir() is not yer implemented for simplescalar */


	/* we handle only one case: 
	 * translate anonymous mmap into allocate
	 * although it is possible to support
	 */
	case SS_old_mmap:
	case SS_SYS_mmap2:
	{
	  	unsigned long addr;
		size_t length;
		int prot,flags,fd;

		//warn("mmap called,try to translate into malloc\n");
		
		/* copy arguments to host memroy */
		mem_bcopy(mem_fn, mem, Read, regs->regs_R[29]+16, 
		  &fd, sizeof(fd));

		/* translate from target format to host format */
		fd = MD_SWAPW(fd);
		length = MD_SWAPW(regs->regs_R[5]);
		prot   = MD_SWAPW(regs->regs_R[6]);
		flags  = MD_SWAPW(regs->regs_R[7]);

		/* MAP_FIXED == 0x10 for both x86 and mips */
		if (regs->regs_R[4]!=0 || fd!=-1 || (flags&0x10)) {
	    	  warn("file mmap is not yet implemented for simplescalar...");
		  /* got an error, indicate result */
		  regs->regs_R[2] = errno;
		  regs->regs_R[7] = 1;
		  break;
		}

		if (length < 0 || length > 0x10000000) {
		  warn("Invalid mmap size %ld\n",length);
		  regs->regs_R[2] = errno;
		  regs->regs_R[7] = 1;
		  break;
		}

		addr = st->mmap_base + st->mmap_size;

		st->mmap_size += length;

		/* page aligned */
        st->mmap_size = (st->mmap_size + MD_PAGE_SIZE - 1) & ( ~ (MD_PAGE_SIZE - 1));

		regs->regs_R[2] = addr;

		regs->regs_R[7] = 0;
	}
	break;

	case SS_SYS_munmap:
#if 0
	{
	  /*result*/regs->regs_R[2] = munmap(/*addr*/regs->regs_R[4],
	      /*len*/regs->regs_R[5]);
	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, indicate result */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
#else
        regs->regs_R[7] = 0;
#endif

	break;


	case SS_SYS_truncate:
	{
		char path[MAXBUFSIZE];
		
		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*path*/regs->regs_R[4], path);
		
        /* truncate the file */
		/*result*/regs->regs_R[2] = truncate(path, regs->regs_R[5]);
        
        /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_ftruncate:
	{
		/*result*/regs->regs_R[2] = ftruncate(/*fd*/regs->regs_R[4], 
		                                  /*length*/regs->regs_R[5]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_fchmod:
	{
		/*result*/regs->regs_R[2] = fchmod(/*fd*/regs->regs_R[4], 
		                                  /*mode*/(half_t)regs->regs_R[5]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_fchown:
	{
		/*result*/regs->regs_R[2] = fchown(/*fd*/regs->regs_R[4], 
										/*user*/(word_t)regs->regs_R[5],
			                             /*group*/(word_t)regs->regs_R[6]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}


	case SS_SYS_getpriority:
	{
		/*result*/regs->regs_R[2] = getpriority(/*which*/regs->regs_R[4],
											/*who*/regs->regs_R[5]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_setpriority:
	{
		/*result*/regs->regs_R[2] = setpriority(/*which*/regs->regs_R[4],
											/*who*/regs->regs_R[5],
											/*prio*/regs->regs_R[6]);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_statfs:
	{
		char path[MAXBUFSIZE];
		struct statfs buf;
		struct ss_statfs ss_buf;
		int i;
		
		/* copy the arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*path*/regs->regs_R[4], path);
		
		/*result*/regs->regs_R[2] = statfs(path, &buf);

		/* translate from host stat structure to target format */
		ss_buf.ss_f_type = MD_SWAPW(buf.f_type);
		ss_buf.ss_f_bsize = MD_SWAPW(buf.f_bsize);
		ss_buf.ss_f_blocks = MD_SWAPW(buf.f_blocks);
		ss_buf.ss_f_bfree = MD_SWAPW(buf.f_bfree);
		ss_buf.ss_f_files = MD_SWAPW(buf.f_files);
		ss_buf.ss_f_ffree = MD_SWAPW(buf.f_ffree);
		ss_buf.ss_f_bavail = MD_SWAPW(buf.f_bavail);
#if defined(__KERNEL__) || defined(__USE_ALL)
		ss_buf.ss_f_fsid.val[0] = MD_SWAPW(buf.f_fsid.val[0]);
		ss_buf.ss_f_fsid.val[1] = MD_SWAPW(buf.f_fsid.val[1]);
#else
		ss_buf.ss_f_fsid.val[0] = MD_SWAPW(buf.f_fsid.__val[0]);
		ss_buf.ss_f_fsid.val[1] = MD_SWAPW(buf.f_fsid.__val[1]);
#endif
		ss_buf.ss_f_namelen = MD_SWAPW(buf.f_namelen);

		for (i=0; i<6; i++)
		  ss_buf.ss_f_spare[i] = MD_SWAPW(buf.f_spare[i]);
			
		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*buf*/regs->regs_R[5], 
		  &ss_buf, sizeof(struct ss_statfs));

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_fstatfs:
	{
		struct statfs buf;
		struct ss_statfs ss_buf;
		int i;
		
		/*result*/regs->regs_R[2] = fstatfs(/*fd*/regs->regs_R[4], &buf);

		/* translate from host stat structure to target format */
		ss_buf.ss_f_type = MD_SWAPW(buf.f_type);
		ss_buf.ss_f_bsize = MD_SWAPW(buf.f_bsize);
		ss_buf.ss_f_blocks = MD_SWAPW(buf.f_blocks);
		ss_buf.ss_f_bfree = MD_SWAPW(buf.f_bfree);
		ss_buf.ss_f_files = MD_SWAPW(buf.f_files);
		ss_buf.ss_f_ffree = MD_SWAPW(buf.f_ffree);
		ss_buf.ss_f_bavail = MD_SWAPW(buf.f_bavail);
#if defined(__KERNEL__) || defined(__USE_ALL)
		ss_buf.ss_f_fsid.val[0] = MD_SWAPW(buf.f_fsid.val[0]);
		ss_buf.ss_f_fsid.val[1] = MD_SWAPW(buf.f_fsid.val[1]);
#else
		ss_buf.ss_f_fsid.val[0] = MD_SWAPW(buf.f_fsid.__val[0]);
		ss_buf.ss_f_fsid.val[1] = MD_SWAPW(buf.f_fsid.__val[1]);
#endif
		ss_buf.ss_f_namelen = MD_SWAPW(buf.f_namelen);

		for (i=0; i<6; i++)
		  ss_buf.ss_f_spare[i] = MD_SWAPW(buf.f_spare[i]);
			
		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*buf*/regs->regs_R[5], 
		  &ss_buf, sizeof(struct ss_statfs));

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;

/* syscall ioperm() 4101 is not implemented for MIPS... */

#if 0
	case SS_SYS_socketcall:
	{
		unsigned long * args;

		args = (unsigned long*)malloc(sizeof(unsigned long));
		if (!args)
		  fatal("out of virtual memory in SS_SYS_socketcall");

		/* copy the arguments to the host memory */
		mem_bcopy(mem_fn, mem, Read, /*args*/regs->regs_R[5], 
		  args, sizeof(unsigned long));

		/*result*/regs->regs_R[2] = socketcall(/*call*/regs->regs_R[4], args);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;
#endif


	case SS_SYS_syslog:
	{
		char buf[MAXBUFSIZE];
		
		/*result*/regs->regs_R[2] = syslog(/*type*/regs->regs_R[4], buf,
			                            /*len*/regs->regs_R[6]);

		/* copy the result to the target memory */
		mem_strcpy(mem_fn, mem, Write, /*buf*/regs->regs_R[5], buf);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


	case SS_SYS_setitimer:
	{
		struct itimerval value, ovalue;
		struct ss_itimerval ss_value, ss_ovalue;

		/* copy arguments to the host memory */
		mem_bcopy(mem_fn, mem, Read, /*value*/regs->regs_R[5],
		  &ss_value, sizeof(struct ss_itimerval));

		/* translate from target itimerval structure to host format */
		value.it_interval.tv_sec = MD_SWAPW(ss_value.ss_it_interval.ss_tv_sec);
		value.it_interval.tv_usec = MD_SWAPW(ss_value.ss_it_interval.ss_tv_usec);
		value.it_value.tv_sec = MD_SWAPW(ss_value.ss_it_value.ss_tv_sec);
		value.it_value.tv_usec = MD_SWAPW(ss_value.ss_it_value.ss_tv_usec);
		
		/*result*/regs->regs_R[2] = setitimer(/*which*/regs->regs_R[4], &value, &ovalue);

		/* translate from host itimerval structure to target format */
		ss_ovalue.ss_it_interval.ss_tv_sec = MD_SWAPW(ovalue.it_interval.tv_sec);
		ss_ovalue.ss_it_interval.ss_tv_usec = MD_SWAPW(ovalue.it_interval.tv_usec);
		ss_ovalue.ss_it_value.ss_tv_sec = MD_SWAPW(ovalue.it_value.tv_sec);
		ss_ovalue.ss_it_value.ss_tv_usec = MD_SWAPW(ovalue.it_value.tv_usec);		

		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*ovalue*/regs->regs_R[6],
		  &ss_ovalue, sizeof(struct ss_itimerval));

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;
	


case SS_SYS_getitimer:
	{
		struct itimerval value;
		struct ss_itimerval ss_value;

		/*result*/regs->regs_R[2] = getitimer(/*which*/regs->regs_R[4], &value);

		/* translate from host itimerval structure to target format */
		ss_value.ss_it_interval.ss_tv_sec = MD_SWAPW(value.it_interval.tv_sec);
		ss_value.ss_it_interval.ss_tv_usec = MD_SWAPW(value.it_interval.tv_usec);
		ss_value.ss_it_value.ss_tv_sec = MD_SWAPW(value.it_value.tv_sec);
		ss_value.ss_it_value.ss_tv_usec = MD_SWAPW(value.it_value.tv_usec);		

		/* copy the result to the target memory */
		mem_bcopy(mem_fn, mem, Write, /*ovalue*/regs->regs_R[5],
		  &ss_value, sizeof(struct ss_itimerval));

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


case SS_SYS_newstat:
      {
	char buf[MAXBUFSIZE];
	struct ss_statbuf ss_sbuf;
	struct stat sbuf;
	
	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fName*/regs->regs_R[4], buf);

	/* stat() the file */
	/*result*/regs->regs_R[2] = stat(buf, &sbuf);
	
	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate from host stat structure to target format */
	ss_sbuf.ss_st_dev = (word_t)MD_SWAPH(sbuf.st_dev);
	ss_sbuf.ss_st_pad1[0] = ss_sbuf.ss_st_pad1[2] = ss_sbuf.ss_st_pad1[2] = 0;
	ss_sbuf.ss_st_ino = MD_SWAPW(sbuf.st_ino);
	ss_sbuf.ss_st_mode = (word_t)MD_SWAPH(sbuf.st_mode);
	ss_sbuf.ss_st_nlink = (word_t)MD_SWAPH(sbuf.st_nlink);
	ss_sbuf.ss_st_uid = (word_t)MD_SWAPH(sbuf.st_uid);
	ss_sbuf.ss_st_gid = (word_t)MD_SWAPH(sbuf.st_gid);
	ss_sbuf.ss_st_rdev = (word_t)MD_SWAPH(sbuf.st_rdev);
	ss_sbuf.ss_st_pad2[0] = ss_sbuf.ss_st_pad2[1] = 0;
	ss_sbuf.ss_st_size = (t_long_t)MD_SWAPW(sbuf.st_size);
	ss_sbuf.ss_st_pad3 = 0;
#if 0
	ss_sbuf.ss_st_atime = (t_long_t)MD_SWAPW(sbuf.st_atime);
	ss_sbuf.ss_st_mtime = (t_long_t)MD_SWAPW(sbuf.st_mtime);
	ss_sbuf.ss_st_ctime = (t_long_t)MD_SWAPW(sbuf.st_ctime);
#endif

	ss_sbuf.ss_reserved0 = ss_sbuf.ss_reserved1 = ss_sbuf.ss_reserved2 = 0;
	ss_sbuf.ss_st_blksize = (t_long_t)MD_SWAPW(sbuf.st_blksize);
	ss_sbuf.ss_st_blocks = (t_long_t)MD_SWAPW(sbuf.st_blocks);
	memset(ss_sbuf.ss_st_pad4, 0, 14*sizeof(t_long_t));
	
	/* copy stat() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*sbuf*/regs->regs_R[5],
		  &ss_sbuf, sizeof(struct ss_statbuf));
      }
      break;

case SS_SYS_newlstat:
      {
	char buf[MAXBUFSIZE];
	struct ss_statbuf ss_sbuf;
	struct stat sbuf;
	
	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fName*/regs->regs_R[4], buf);

	/* lstat() the file */
	/*result*/regs->regs_R[2] = lstat(buf, &sbuf);
	
	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate from host stat structure to target format */
	ss_sbuf.ss_st_dev = (word_t)MD_SWAPH(sbuf.st_dev);
	ss_sbuf.ss_st_pad1[0] = ss_sbuf.ss_st_pad1[2] = ss_sbuf.ss_st_pad1[2] = 0;
	ss_sbuf.ss_st_ino = MD_SWAPW(sbuf.st_ino);
	ss_sbuf.ss_st_mode = (word_t)MD_SWAPH(sbuf.st_mode);
	ss_sbuf.ss_st_nlink = (word_t)MD_SWAPH(sbuf.st_nlink);
	ss_sbuf.ss_st_uid = (word_t)MD_SWAPH(sbuf.st_uid);
	ss_sbuf.ss_st_gid = (word_t)MD_SWAPH(sbuf.st_gid);
	ss_sbuf.ss_st_rdev = (word_t)MD_SWAPH(sbuf.st_rdev);
	ss_sbuf.ss_st_pad2[0] = ss_sbuf.ss_st_pad2[1] = 0;
	ss_sbuf.ss_st_size = (t_long_t)MD_SWAPW(sbuf.st_size);
	ss_sbuf.ss_st_pad3 = 0;
#if 0
	ss_sbuf.ss_st_atime = (t_long_t)MD_SWAPW(sbuf.st_atime);
	ss_sbuf.ss_st_mtime = (t_long_t)MD_SWAPW(sbuf.st_mtime);
	ss_sbuf.ss_st_ctime = (t_long_t)MD_SWAPW(sbuf.st_ctime);
#endif
	
	ss_sbuf.ss_reserved0 = ss_sbuf.ss_reserved1 = ss_sbuf.ss_reserved2 = 0;
	ss_sbuf.ss_st_blksize = (t_long_t)MD_SWAPW(sbuf.st_blksize);
	ss_sbuf.ss_st_blocks = (t_long_t)MD_SWAPW(sbuf.st_blocks);
	memset(ss_sbuf.ss_st_pad4, 0, 14*sizeof(t_long_t));
	
	/* copy lstat() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*sbuf*/regs->regs_R[5],
		  &ss_sbuf, sizeof(struct ss_statbuf));
      }
      break;


case SS_SYS_newfstat:
      {
	struct ss_statbuf ss_sbuf;
	struct stat sbuf;
	

	/* fstat() the file */
	/*result*/regs->regs_R[2] = fstat(/*fd*/regs->regs_R[4], &sbuf);
	
	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate from host stat structure to target format */
	ss_sbuf.ss_st_dev = (word_t)MD_SWAPH(sbuf.st_dev);
	ss_sbuf.ss_st_pad1[0] = ss_sbuf.ss_st_pad1[2] = ss_sbuf.ss_st_pad1[2] = 0;
	ss_sbuf.ss_st_ino = MD_SWAPW(sbuf.st_ino);
	ss_sbuf.ss_st_mode = (word_t)MD_SWAPH(sbuf.st_mode);
	ss_sbuf.ss_st_nlink = (word_t)MD_SWAPH(sbuf.st_nlink);
	ss_sbuf.ss_st_uid = (word_t)MD_SWAPH(sbuf.st_uid);
	ss_sbuf.ss_st_gid = (word_t)MD_SWAPH(sbuf.st_gid);
	ss_sbuf.ss_st_rdev = (word_t)MD_SWAPH(sbuf.st_rdev);
	ss_sbuf.ss_st_pad2[0] = ss_sbuf.ss_st_pad2[1] = 0;
	ss_sbuf.ss_st_size = (t_long_t)MD_SWAPW(sbuf.st_size);
	ss_sbuf.ss_st_pad3 = 0;
#if 0
	ss_sbuf.ss_st_atime = (t_long_t)MD_SWAPW(sbuf.st_atime);
	ss_sbuf.ss_st_mtime = (t_long_t)MD_SWAPW(sbuf.st_mtime);
	ss_sbuf.ss_st_ctime = (t_long_t)MD_SWAPW(sbuf.st_ctime);
#endif
	
	ss_sbuf.ss_reserved0 = ss_sbuf.ss_reserved1 = ss_sbuf.ss_reserved2 = 0;
	ss_sbuf.ss_st_blksize = (t_long_t)MD_SWAPW(sbuf.st_blksize);
	ss_sbuf.ss_st_blocks = (t_long_t)MD_SWAPW(sbuf.st_blocks);
	memset(ss_sbuf.ss_st_pad4, 0, 14*sizeof(t_long_t));
	
	/* copy lstat() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*sbuf*/regs->regs_R[5],
		  &ss_sbuf, sizeof(struct ss_statbuf));
      }
      break;


      case SS_SYS_uname:
      {
	struct old_utsname *name;

	name = (struct old_utsname *)malloc(sizeof(struct old_utsname));
	if (!name)
	  fatal("out of virtual memory in SYS_uname");

	/*result*/regs->regs_R[2] = uname(name);

	/* copy host side memory into target side pointer data */
	mem_bcopy(mem_fn, mem, Write, regs->regs_R[4],
	    name, sizeof(struct old_utsname));

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}

	/* free the host memory */
	free(name);
      }
      break;

      /* syscall SS_SYS_iopl is not yet implemented for MIPS... */

      case SS_SYS_vhangup:
      {
	/*result*/regs->regs_R[2] = vhangup();

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
      }
      break;

/* syscall SS_SYS_vm86 is not yet implemented for MIPS.. */	

/* syscall SS_SYS_wait4 is not yet implemented for simplescalar *
 * simplescalar does not support multithreads                  */

#if 0
	case SS_SYS_swapoff:
	{
	  char buf[MAXBUFSIZE];

	  /* copy arguments to the host memory */
	  mem_strcpy(mem_fn, mem, Read, /*path*/regs->regs_R[4], buf);

	  /*result*/regs->regs_R[2] = swapoff(buf);

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;
#endif


	case SS_SYS_sysinfo:
	{
	  struct sysinfo infobuf;
	  struct ss_sysinfo ss_infobuf;
	  int i;
	  
	  /*result*/regs->regs_R[2] = sysinfo(&infobuf);

	  /* translate from host sysinof structure to target format */
	  ss_infobuf.ss_uptime = MD_SWAPW(infobuf.uptime);
	  for(i=0; i<3; i++)
	    ss_infobuf.ss_loads[i] = MD_SWAPW(infobuf.loads[i]);
	  ss_infobuf.ss_totalram = MD_SWAPW(infobuf.totalram);
	  ss_infobuf.ss_freeram = MD_SWAPW(infobuf.freeram);
	  ss_infobuf.ss_sharedram = MD_SWAPW(infobuf.sharedram);
	  ss_infobuf.ss_bufferram = MD_SWAPW(infobuf.bufferram);
	  ss_infobuf.ss_totalswap = MD_SWAPW(infobuf.totalswap);
	  ss_infobuf.ss_freeswap = MD_SWAPW(infobuf.freeswap);
	  ss_infobuf.ss_procs = MD_SWAPH(infobuf.procs);
	  ss_infobuf.ss_pad = MD_SWAPH(infobuf.pad);
	  ss_infobuf.ss_totalhigh = MD_SWAPW(infobuf.totalhigh);
	  ss_infobuf.ss_freehigh = MD_SWAPW(infobuf.freehigh);
	  ss_infobuf.ss_mem_unit = MD_SWAPW(infobuf.mem_unit);
	  for(i=0; i<(20-2*sizeof(t_long_t)-sizeof(int)); i++)
	  	ss_infobuf.ss_f[i] = infobuf._f[1];

	  /* copy the result to the target memory */
	  mem_bcopy(mem_fn, mem, Write, /*info*/regs->regs_R[4],
	    &ss_infobuf, sizeof(struct ss_sysinfo));

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


/* syscall SS_SYS_ipc is not yet implemented for simplescalar... */

	case SS_SYS_fsync:
	{
	  /*result*/regs->regs_R[2] = fsync(/*fd*/regs->regs_R[4]);

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;

/* syscall SS_SYS_sigreturn is not yet implemented for simplescalar... */

/* syscall SS_SYS_clone is not yet implemented for simplecalar... *
 * simplescalar does not suopport multithreads                  */


	case SS_SYS_setdomainname:
	{
	  char buf[MAXBUFSIZE];

	  /* copy arguments to the host memory */
	  mem_strcpy(mem_fn, mem, Read, /*name*/regs->regs_R[4], buf);

	  /*result*/regs->regs_R[2] = setdomainname(buf, /*len*/regs->regs_R[5]);

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;

	
	case SS_SYS_newuname:
	{
		struct new_utsname *name;

		name = (struct new_utsname *)malloc(sizeof(struct new_utsname));
		if (!name)
	      fatal("out of virtual memory in SYS_newuname");

		/*result*/regs->regs_R[2] = uname(name);

		/* copy host side memory into target side pointer data */
		mem_bcopy(mem_fn, mem, Write, regs->regs_R[4],
		  name, sizeof(struct new_utsname));

		if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
		else
		  {
			regs->regs_R[2] = errno;
		    regs->regs_R[7] = 1;
		  }
		/* free the host memory */
		free(name);
	}
	break;


	case SS_SYS_adjtimex:
	{
	  struct timex txc;
	  struct ss_timex ss_txc;

	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, /*txc*/regs->regs_R[4],
	    &txc, sizeof(struct timex));

	  /* translate from target timex structure to host format */
	  txc.modes = MD_SWAPW(txc.modes);
	  txc.offset = MD_SWAPW(txc.offset);
	  txc.freq = MD_SWAPW(txc.freq);
	  txc.maxerror = MD_SWAPW(txc.esterror);
	  txc.status = MD_SWAPW(txc.status);
	  txc.constant = MD_SWAPW(txc.constant);
	  txc.precision = MD_SWAPW(txc.precision);
	  txc.tolerance = MD_SWAPW(txc.tolerance);
	  txc.time.tv_sec = MD_SWAPW(txc.time.tv_sec);
	  txc.time.tv_usec = MD_SWAPW(txc.time.tv_usec);
	  txc.tick = MD_SWAPW(txc.tick);

	  /*result*/regs->regs_R[2] = adjtimex(&txc);

	  /* translate from host timex structure to target format */
	  ss_txc.ss_modes = MD_SWAPW(txc.modes);
	  ss_txc.ss_offset = MD_SWAPW(txc.offset);
	  ss_txc.ss_freq = MD_SWAPW(txc.freq);
	  ss_txc.ss_maxerror = MD_SWAPW(txc.esterror);
	  ss_txc.ss_status = MD_SWAPW(txc.status);
	  ss_txc.ss_constant = MD_SWAPW(txc.constant);
	  ss_txc.ss_precision = MD_SWAPW(txc.precision);
	  ss_txc.ss_tolerance = MD_SWAPW(txc.tolerance);
	  ss_txc.ss_time.ss_tv_sec = MD_SWAPW(txc.time.tv_sec);
	  ss_txc.ss_time.ss_tv_usec = MD_SWAPW(txc.time.tv_usec);
	  ss_txc.ss_tick = MD_SWAPW(txc.tick);

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


/* syscall SS_SYS_mproctect is not yet implemented for simplescalar... */

#if 0 /* broken, the whole signal handling need recheck */
	case SS_SYS_sigprocmask:
	{
		mips_old_sigset_t *set, *oset;
		
		set = (mips_old_sigset_t *)malloc(sizeof(mips_old_sigset_t));
		oset = (mips_old_sigset_t *)malloc(sizeof(mips_old_sigset_t));
		if ((!set)||(!oset))
		  fatal("out of virtual memory in SYS_sigprocmask");

		/* copy target side pointer data into host side memory */
		mem_bcopy(mem_fn, mem, Read, regs->regs_R[5],
		  set, sizeof(mips_old_sigset_t));
		
		/* translate to host format */
		*set = MD_SWAPW(*set);
		
		/*result*/regs->regs_R[2] = sigprocmask(/*how*/regs->regs_R[4], set, oset);
		
		/* translate to target format */
		*oset = MD_SWAPW(*oset);

		/* copy host side memory into target side pointer data */
		mem_bcopy(mem_fn, mem, Write, regs->regs_R[6],
		  oset, sizeof(mips_old_sigset_t));

		if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
		else
		{
			regs->regs_R[2] = errno;
			regs->regs_R[7] = 1;
		}

		/* free the host memory */
		free(set);
		free(oset);
	}
	break;
#endif

#if 0
	case SS_SYS_create_module:
	{
	  char buf[MAXBUFSIZE];

	  /* copy arguments to the host memory */
	  mem_strcpy(mem_fn, mem, Read, /*name_user*/regs->regs_R[4], buf);

	  /*result*/regs->regs_R[2] = create_module(buf, /*size*/regs->regs_R[5]);

	  /* check for an error condition */
	  if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
	  else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;
#endif

	
/* syscall SS_SYS_create_module SS_SYS_init_module SS_SYS_delete_module
 * SS_SYS_query_module SS_SYS_get_kernel_syms SS_SYS_quotactl are not 
 * yet implemented for simplescalar... */


	case SS_SYS_getpgid:
#ifdef _MSC_VER
      warn("syscall getpgid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*result*/regs->regs_R[2] = getpgid(/*pid*/regs->regs_R[4]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
#endif /* _MSC_VER */
    break;   
		

	case SS_SYS_fchdir:
	{
	  /*result*/regs->regs_R[2] = fchdir(/*fd*/regs->regs_R[4]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


/* syscall SS_SYS_bdflush SS_SYS_sysfs are not yet implemented for simplscalar... */


	case SS_SYS_personality:
	{
	  /*result*/regs->regs_R[2] = personality(/*personality*/regs->regs_R[4]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
	break;


	case SS_SYS_setfsuid:
#ifdef _MSC_VER
      warn("syscall setfsuid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*result*/regs->regs_R[2] = setfsuid(/*uid*/regs->regs_R[4]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
#endif /* _MSC_VER */
    break;   


	case SS_SYS_setfsgid:
#ifdef _MSC_VER
      warn("syscall setfsgid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /*!_MSC_VER_*/
	  /*result*/regs->regs_R[2] = setfsgid(/*uid*/regs->regs_R[4]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
#endif /* _MSC_VER */
    break;   


#if 1
	case SS_SYS_llseek:
   	{
	    sqword_t offset64,result;
		word_t origin;

#if 0
        /* this argument is not used, since the very syscall on i386 platform  *
         * does not need this argument                                   */
         
		sqword_t * result;
		result = (sqword_t*)malloc(sizeof(sqword_t));
		if (!result)
		  fatal("out of virtual memory in SS_SYS_llseek");

		mem_bcopy(mem_fn, mem, Read, /*result*/regs->regs_R[7],
		  result, sizeof(sqword_t));
#endif

		mem_bcopy(mem_fn, mem, Read, /*origin*/regs->regs_R[29]+16,
		  &origin, sizeof(word_t));
		
		/* translate to host format */
		origin = MD_SWAPW(origin);
		
#if 0
            offset64 = ((sqword_t)regs->regs_R[5]<<32)|(sqword_t)regs->regs_R[6];

	    /*result*/regs->regs_R[2] = lseek64(/*fd*/regs->regs_R[4], offset64, origin);
#else
            //myfprintf(stderr,"llseek: fd=%d,offset=(%x,%x),origin=%d\n", regs->regs_R[4],regs->regs_R[5],regs->regs_R[6],origin);
            if (regs->regs_R[5]!=0) {
              myfprintf(stderr,"No support for > 4G file!\n");
              regs->regs_R[2] = 0;
            }else {
	      /*result*/regs->regs_R[2] = lseek(/*fd*/regs->regs_R[4],(off_t)regs->regs_R[6], origin);
            }

#endif
	
	   /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
              myfprintf(stderr,"llseek: fd=%d,offset=(%x,%x),origin=%d\n",
                regs->regs_R[4],regs->regs_R[5],regs->regs_R[6],origin);
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
    }
	break;
#endif


/* syscall SS_SYS_getdents is not yet implemented for simplescalar... */

	case SS_SYS_flock:
	{
	  /*result*/regs->regs_R[2] = flock(/*fd*/regs->regs_R[4], /*cmd*/regs->regs_R[5]);

	  /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;


/* syscall SS_SYS_msync is not yet implemented for simplescalar... */


	case SS_SYS_readv:
#ifdef _MSC_VER
      warn("syscall readv() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
	{
	  int i;
	  char *buf;
	  word_t *address;
          struct iovec *iov;
      
	  /* allocate host side I/O vectors */
	  iov = (struct iovec*)malloc(/*iovcnt*/regs->regs_R[6]
	  					* sizeof(struct iovec));
	  if (!iov)
	  	fatal("out of virtual memory in SS_SYS_readv");

	  /* allocate address buffer to hold the target buffer address*/
	  address = (word_t*)malloc(/*iovcnt*/regs->regs_R[6] 
	  							* sizeof(word_t));
	  if (!address)
	  	fatal("out of virtual memory in SS_SYS_readv");
	  
	  /* copy target side pointer data into host side vector */
	  mem_bcopy(mem_fn, mem, Read, /*iov*/regs->regs_R[5],
		  iov, /*iovcnt*/regs->regs_R[6] * sizeof(struct iovec)); 
	  
	  for (i=0; i</*iovcnt*/regs->regs_R[6]; i++)
	  {
	    iov[i].iov_base = MD_SWAPW((unsigned)iov[i].iov_base);
	    iov[i].iov_len = MD_SWAPW(iov[i].iov_len);
	    if (iov[i].iov_base != NULL)
	    {
	      address[i] = iov[i].iov_base;
	      buf = (char *)malloc(iov[i].iov_len);
	      if (!buf)
		fatal ("out of virtual memroy in SS_SYS_readv");
	      iov[i].iov_base = (void *)buf;
	    }
	    else
	      address[i] = NULL;
	  }

	  /* perform the vector'ed read */
	  /*result*/regs->regs_R[2] = readv(/*fd*/regs->regs_R[4], iov,
	  									/*iovcnt*/regs->regs_R[6]);

      /* copy results to the target memory and free all the buffers */
	  for (i=0; i</*iovcnt*/regs->regs_R[6]; i++)
	  {
		if (address[i] != NULL)
		{
		  mem_bcopy(mem_fn, mem, Write, address[i], 
		    (char*)iov[i].iov_base, iov[i].iov_len);
		  free(iov[i].iov_base);
		  iov[i].iov_base = NULL;
		}
	  }
	  free(address);
	  free(iov);

	  /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
#endif /*!_MSC_VER*/
	break;


/* syscall SS_SYS_cacheflush is not yet implemented for simplescalar... */

/* syscall SS_SYS_cachectl and SS_SYS_sysmips are not yet implemented for MIPS... */

	case SS_SYS_getsid:
#ifdef _MSC_VER
      warn("syscall getsid() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
    
      /* result */regs->regs_R[2] = getsid(/*pid*/regs->regs_R[4]);

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /* _MSC_VER */
      break;


	case SS_SYS_fdatasync:
	{
	  /*result*/regs->regs_R[2] = fdatasync(/*fd*/regs->regs_R[4]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
	}
    break;

/* syscall SS_SYS_sysctl is not yet implemented for simplescalar... */

/* syscall SS_SYS_mlock SS_SYS_munlock SS_SYS_mlockall SS_SYS_munlockall *
 * are not yet implemented for simplescalar                                */


	case SS_SYS_sched_setparam:
	{
	  struct sched_param param;
	  struct ss_sched_param ss_param;

	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, /*praam*/regs->regs_R[5],
	    &param, sizeof(struct sched_param));

	  /* translate from host sched_param structure to target format */
	  ss_param.ss_sched_priority = MD_SWAPW(param.sched_priority);

	  /*result*/regs->regs_R[2] =  sched_setparam(/*pid*/regs->regs_R[4],
	  											&ss_param);
	
	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;



	case SS_SYS_sched_getparam:
	{
	  struct sched_param param;
	  struct ss_sched_param ss_param;

	  /*result*/regs->regs_R[2] =  sched_getparam(/*pid*/regs->regs_R[4],
	  											&param);
	  
	  /* translate from host sched_param structure to target format */
	  ss_param.ss_sched_priority = MD_SWAPW(param.sched_priority);

	  /* copy results to the target memory */
	  mem_bcopy(mem_fn, mem, Write, /*praam*/regs->regs_R[5],
	    &ss_param, sizeof(struct ss_sched_param));
	  
	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_sched_setscheduler:
	{
	  struct sched_param param;
	  struct ss_sched_param ss_param;

	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, /*praam*/regs->regs_R[6],
	    &param, sizeof(struct sched_param));

	  /* translate from host sched_param structure to target format */
	  ss_param.ss_sched_priority = MD_SWAPW(param.sched_priority);

	  /*result*/regs->regs_R[2] =  sched_setscheduler(/*pid*/regs->regs_R[4],
	  											/*policy*/regs->regs_R[5],
	  											&ss_param);
	
	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_sched_getscheduler:
	{
	  /*result*/regs->regs_R[2] = sched_getscheduler(/*pid*/regs->regs_R[4]);

	   /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_sched_yield:
	{
	  /*result*/regs->regs_R[2] = sched_yield();

	   /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_sched_get_priority_max:
	{
	  /*result*/regs->regs_R[2] = sched_get_priority_max(/*policy*/regs->regs_R[4]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_sched_get_priority_min:
    {
      /*result*/regs->regs_R[2] = sched_get_priority_min(/*policy*/regs->regs_R[4]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


/* syscall SS_SYS_sched_rr_get_interval is not yet implemented for simplescalar... */
/* syscall SS_SYS_nanosleep is not yet implemented for simplescalar... */


	case SS_SYS_mremap:
	{
	  warn("syscall SS_SYS_mremap not yet implemented");
	  regs->regs_R[2] = 0;
	  regs->regs_R[7] = 0;
	}
	break;


	case SS_SYS_getsockopt:
	{
	  char buf[MAXBUFSIZE];
	  sword_t * buflen;
	  word_t address;
		
	  buflen = (sword_t*)malloc(sizeof(sword_t));
	  if (!buflen)
	  	fatal("out of virtual memory in SS_SYS_getsockopt");
	  
	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, regs->regs_R[29]+16,
	    &address, sizeof(word_t));

	  /* translate from target format to host format */
	  address = MD_SWAPW(address);

	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, address, buflen, sizeof(sword_t));

	  /* translate from target format to host format */
	  *buflen = MD_SWAPW(*buflen);

	  /*result*/regs->regs_R[2] = getsockopt(/*fd*/regs->regs_R[4], 
	  	                                  /*level*/regs->regs_R[5],
	  	                                  /*optname*/regs->regs_R[6], 
	  	                                  buf, buflen);

	  /* translate from host format to target format */
	  *buflen = MD_SWAPW(*buflen);
	  address = MD_SWAPW(address);
	  
	  /* copy the result to the target memory */
	  mem_strcpy(mem_fn, mem, Write, regs->regs_R[7], buf);
	  mem_bcopy(mem_fn, mem, Write, address, buflen, sizeof(sword_t));

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      free(buflen);
	}
	break;



	case SS_SYS_setsockopt:
	{
	  char buf[MAXBUFSIZE];
	  sword_t buflen;
	  
	  
	  /* copy arguments to the host memory */
	  mem_strcpy(mem_fn, mem, Read, regs->regs_R[7], buf);
	  mem_bcopy(mem_fn, mem, Read, regs->regs_R[29]+16,
	    &buflen, sizeof(sword_t));

	  /* translate from target format to host format */
	  buflen = MD_SWAPW(buflen);

	  /*result*/regs->regs_R[2] = setsockopt(/*fd*/regs->regs_R[4], 
	  	                                  /*level*/regs->regs_R[5],
	  	                                  /*optname*/regs->regs_R[6], 
	  	                                  buf, buflen);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_setresuid:
	{
	  /*result*/regs->regs_R[2] = setresuid(/*ruid*/(word_t)regs->regs_R[4],
	  									/*euid*/(word_t)regs->regs_R[5],
	  									/*suid*/(word_t)regs->regs_R[6]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;

	case SS_SYS_getresuid:
	{
	  word_t *ruid, *euid, *suid;

	  ruid = (word_t*)malloc(sizeof(word_t));
	  euid = (word_t*)malloc(sizeof(word_t));
	  suid = (word_t*)malloc(sizeof(word_t));
	  if ((!ruid)||(!euid)||(!suid))
	  	fatal("out of virtual memroy in SS_SYS_getresuid");

	  /*result*/regs->regs_R[2] = getresuid(ruid, euid, suid);

	  /* translate from host format to target format */
	  *ruid = MD_SWAPW((sword_t)*ruid);
	  *euid = MD_SWAPW((sword_t)*euid);
	  *suid = MD_SWAPW((sword_t)*suid);

	  /* copy results to the target memory */
	  mem_bcopy(mem_fn, mem, Write, /*ruid*/regs->regs_R[4],
	    ruid, sizeof(sword_t));
	  mem_bcopy(mem_fn, mem, Write, /*euid*/regs->regs_R[5],
	    euid, sizeof(sword_t));
	  mem_bcopy(mem_fn, mem, Write, /*suid*/regs->regs_R[6],
	    suid, sizeof(sword_t));

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
      free(ruid);
      free(euid);
      free(suid);
	}
	break;


	case SS_SYS_setresgid:
	{
	  /*result*/regs->regs_R[2] = setresgid(/*rgid*/(word_t)regs->regs_R[4],
	  									/*egid*/(word_t)regs->regs_R[5],
	  									/*sgid*/(word_t)regs->regs_R[6]);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;


	case SS_SYS_getresgid:
	{
	  word_t *rgid, *egid, *sgid;

	  rgid = (word_t*)malloc(sizeof(word_t));
	  egid = (word_t*)malloc(sizeof(word_t));
	  sgid = (word_t*)malloc(sizeof(word_t));
	  if ((!rgid)||(!egid)||(!sgid))
	  	fatal("out of virtual memroy in SS_SYS_getresuid");

	  /*result*/regs->regs_R[2] = getresgid(rgid, egid, sgid);

	  /* translate from host format to target format */
	  *rgid = MD_SWAPW((sword_t)*rgid);
	  *egid = MD_SWAPW((sword_t)*egid);
	  *sgid = MD_SWAPW((sword_t)*sgid);

	  /* copy results to the target memory */
	  mem_bcopy(mem_fn, mem, Write, /*rgid*/regs->regs_R[4],
	    rgid, sizeof(sword_t));
	  mem_bcopy(mem_fn, mem, Write, /*egid*/regs->regs_R[5],
	    egid, sizeof(sword_t));
	  mem_bcopy(mem_fn, mem, Write, /*sgid*/regs->regs_R[6],
	    sgid, sizeof(sword_t));

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
      free(rgid);
      free(egid);
      free(sgid);
	}
	break;


	case SS_SYS_pread:
	{
	  char buf[MAXBUFSIZE];
	  long long pos;

	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, regs->regs_R[29]+16,
	    &pos, sizeof(long long));

	  /* translate from target format to host format */
	  pos = MD_SWAPQ(pos);
	  
	  /*result*/regs->regs_R[2] = pread(/*fd*/regs->regs_R[4], buf,
	  								  /*count*/regs->regs_R[6], pos);

	  /* copy result to the target memory */
	  mem_strcpy(mem_fn, mem, Write, /*buf*/regs->regs_R[5], buf);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;



	case SS_SYS_pwrite:
	{
	  char buf[MAXBUFSIZE];
	  long long pos;

	  /* copy arguments to the host memory */
	  mem_strcpy(mem_fn, mem, Read, regs->regs_R[5], buf);
	  mem_bcopy(mem_fn, mem, Read, regs->regs_R[29]+16,
	    &pos, sizeof(long long));

	  /* translate from target format to host format */
	  pos = MD_SWAPQ(pos);
	  
	  /*result*/regs->regs_R[2] = pwrite(/*fd*/regs->regs_R[4], buf,
	  								  /*count*/regs->regs_R[6], pos);

	  /* check for an error condition */
      if (regs->regs_R[2] != -1)
	    regs->regs_R[7] = 0;
      else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }  
	}
	break;



	case SS_SYS_chown:
#ifdef _MSC_VER
      warn("syscall chown() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	/* chown the file */
	/*result*/regs->regs_R[2] = chown(buf, /*owner*/regs->regs_R[5],
				    /*group*/regs->regs_R[6]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
#endif /* _MSC_VER */
      break;


      case SS_SYS_getcwd:
	  {
		char buf[MAXBUFSIZE];

		/* copy arguments to host memory */
		mem_strcpy(mem_fn, mem, Read, /*buf*/regs->regs_R[4], buf);

		/*result*/regs->regs_R[2] = getcwd(buf, /*size*/regs->regs_R[5]);

	  	/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
      }
	break;


	case SS_SYS_sendfile:
	{
	  long * offset;

	  offset = (long*)malloc(sizeof(long));
	  if (!offset)
	  	fatal("out ot virtual memory in SS_SYS_sendfile");
	  
	  /* copy arguments to the host memory */
	  mem_bcopy(mem_fn, mem, Read, /*offset*/regs->regs_R[6], 
	    offset, sizeof(long));

	  /* translate from target format to host format */
	  *offset = MD_SWAPW(*offset);

	  /*result*/regs->regs_R[2] = sendfile(/*out_fd*/regs->regs_R[4], 
	  									/*in_fd*/regs->regs_R[5], offset,
	  									/*count*/regs->regs_R[7]);
	  /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	    free(offset);
	}
	break;




	case SS_SYS_truncate64:
	{
		char path[MAXBUFSIZE];
		sqword_t offset64;
		
		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, /*path*/regs->regs_R[4],path);
		
		offset64 = ((sqword_t)regs->regs_R[5]<<32)|(sqword_t)regs->regs_R[6];

        /* truncate the file */
		/*result*/regs->regs_R[2] = truncate64(path, offset64);
        
        /* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;

	case SS_SYS_ftruncate64:
	{
		sqword_t offset64;

		offset64 = ((sqword_t)regs->regs_R[5]<<32)|(sqword_t)regs->regs_R[6];
		
		/*result*/regs->regs_R[2] = ftruncate64(/*fd*/regs->regs_R[4], offset64);

		/* check for an error condition */
	    if (regs->regs_R[2] != -1)
	      regs->regs_R[7] = 0;
	    else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;

#if 1

	 case SS_SYS_stat64:
      {
	char buf[MAXBUFSIZE];
	struct ss_statbuf64 ss_sbuf64;
#ifdef _MSC_VER
	struct _stat64 sbuf64;
#else /* !_MSC_VER */
	struct stat64 sbuf64;
#endif /* _MSC_VER */

	/* copy argument to the host memory */
	mem_strcpy(mem_fn, mem, Read, /*filename*/regs->regs_R[4], buf);

	/* stat64() the file */
	/*result*/regs->regs_R[2] = stat64(buf, &sbuf64);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate the stat64 structure to host format */
	ss_sbuf64.ss_st_dev = (t_ulong_t)MD_SWAPW(sbuf64.st_dev);
	memset(ss_sbuf64.ss_pad0, 0, 12*sizeof(byte_t));
	ss_sbuf64.ss_st_ino = MD_SWAPQ(sbuf64.st_ino);
	ss_sbuf64.ss_st_mode = MD_SWAPW(sbuf64.st_mode);
	ss_sbuf64.ss_st_nlink = MD_SWAPW(sbuf64.st_nlink);
	ss_sbuf64.ss_st_uid = MD_SWAPW(sbuf64.st_uid);
	ss_sbuf64.ss_st_gid = MD_SWAPW(sbuf64.st_gid);
	ss_sbuf64.ss_st_rdev = (t_ulong_t)MD_SWAPH(sbuf64.st_rdev);
	memset(ss_sbuf64.ss_pad1, 0, 12*sizeof(byte_t));
	ss_sbuf64.ss_st_size = MD_SWAPQ(sbuf64.st_size);
#if 0
	ss_sbuf64.ss_st_atime = MD_SWAPW(sbuf64.st_atime);
	ss_sbuf64.ss_reserved0 = 0;
	ss_sbuf64.ss_st_mtime = MD_SWAPW(sbuf64.st_mtime);
	ss_sbuf64.ss_reserved1 = 0;
	ss_sbuf64.ss_st_ctime = MD_SWAPW(sbuf64.st_ctime);
	ss_sbuf64.ss_reserved2 = 0;
#endif

	ss_sbuf64.ss_pad2 = 0;
#ifndef _MSC_VER
	ss_sbuf64.ss_st_blksize = MD_SWAPW(sbuf64.st_blksize);
	ss_sbuf64.ss_st_blocks = (long long)MD_SWAPQ(sbuf64.st_blocks);
#endif /* !_MSC_VER */


	/* copy stat64() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*sbuf64*/regs->regs_R[5],
		  &ss_sbuf64, sizeof(struct ss_statbuf64));
      }
      break;

	case SS_SYS_lstat64:
      {
	char buf[MAXBUFSIZE];
	struct ss_statbuf64 ss_sbuf64;
#ifdef _MSC_VER
	struct _stat64 sbuf64;
#else /* !_MSC_VER */
	struct stat64 sbuf64;
#endif /* _MSC_VER */

	/* copy argument to the host memory */
	mem_strcpy(mem_fn, mem, Read, /*filename*/regs->regs_R[4], buf);

	/* lstat64() the file */
	/*result*/regs->regs_R[2] = lstat64(buf, &sbuf64, /*flags*/regs->regs_R[6]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate the stat64 structure to host format */
	ss_sbuf64.ss_st_dev = (t_ulong_t)MD_SWAPW(sbuf64.st_dev);
	memset(ss_sbuf64.ss_pad0, 0, 12*sizeof(byte_t));
	ss_sbuf64.ss_st_ino = MD_SWAPQ(sbuf64.st_ino);
	ss_sbuf64.ss_st_mode = MD_SWAPW(sbuf64.st_mode);
	ss_sbuf64.ss_st_nlink = MD_SWAPW(sbuf64.st_nlink);
	ss_sbuf64.ss_st_uid = MD_SWAPW(sbuf64.st_uid);
	ss_sbuf64.ss_st_gid = MD_SWAPW(sbuf64.st_gid);
	ss_sbuf64.ss_st_rdev = (t_ulong_t)MD_SWAPH(sbuf64.st_rdev);
	memset(ss_sbuf64.ss_pad1, 0, 12*sizeof(byte_t));
	ss_sbuf64.ss_st_size = MD_SWAPQ(sbuf64.st_size);
#if 0
	ss_sbuf64.ss_st_atime = MD_SWAPW(sbuf64.st_atime);
	ss_sbuf64.ss_reserved0 = 0;
	ss_sbuf64.ss_st_mtime = MD_SWAPW(sbuf64.st_mtime);
	ss_sbuf64.ss_reserved1 = 0;
	ss_sbuf64.ss_st_ctime = MD_SWAPW(sbuf64.st_ctime);
	ss_sbuf64.ss_reserved2 = 0;
#endif
	ss_sbuf64.ss_pad2 = 0;
#ifndef _MSC_VER
	ss_sbuf64.ss_st_blksize = MD_SWAPW(sbuf64.st_blksize);
	ss_sbuf64.ss_st_blocks = (long long)MD_SWAPQ(sbuf64.st_blocks);
#endif /* !_MSC_VER */


	/* copy lstat64() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*sbuf64*/regs->regs_R[5],
		  &ss_sbuf64, sizeof(struct ss_statbuf64));
      }
      break;

	case SS_SYS_fstat64:
      {
	struct ss_statbuf64 ss_sbuf64;
#ifdef _MSC_VER
	struct _stat64 sbuf64;
#else /* !_MSC_VER */
	struct stat64 sbuf64;
#endif /* _MSC_VER */

	/* fstat64() the file */
	/*result*/regs->regs_R[2] = fstat64(/*fd*/regs->regs_R[4], &sbuf64, 
									/*flags*/regs->regs_R[6]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* translate the stat64 structure to host format */
	ss_sbuf64.ss_st_dev = (t_ulong_t)MD_SWAPW(sbuf64.st_dev);
	memset(ss_sbuf64.ss_pad0, 0, 12*sizeof(byte_t));
	ss_sbuf64.ss_st_ino = MD_SWAPQ(sbuf64.st_ino);
	ss_sbuf64.ss_st_mode = MD_SWAPW(sbuf64.st_mode);
	ss_sbuf64.ss_st_nlink = MD_SWAPW(sbuf64.st_nlink);
	ss_sbuf64.ss_st_uid = MD_SWAPW(sbuf64.st_uid);
	ss_sbuf64.ss_st_gid = MD_SWAPW(sbuf64.st_gid);
	ss_sbuf64.ss_st_rdev = (t_ulong_t)MD_SWAPH(sbuf64.st_rdev);
	memset(ss_sbuf64.ss_pad1, 0, 12*sizeof(byte_t));
	ss_sbuf64.ss_st_size = MD_SWAPQ(sbuf64.st_size);
#if 0
	ss_sbuf64.ss_st_atime = MD_SWAPW(sbuf64.st_atime);
	ss_sbuf64.ss_reserved0 = 0;
	ss_sbuf64.ss_st_mtime = MD_SWAPW(sbuf64.st_mtime);
	ss_sbuf64.ss_reserved1 = 0;
	ss_sbuf64.ss_st_ctime = MD_SWAPW(sbuf64.st_ctime);
	ss_sbuf64.ss_reserved2 = 0;
#endif
	ss_sbuf64.ss_pad2 = 0;
#ifndef _MSC_VER
	ss_sbuf64.ss_st_blksize = MD_SWAPW(sbuf64.st_blksize);
	ss_sbuf64.ss_st_blocks = (long long)MD_SWAPQ(sbuf64.st_blocks);
#endif /* !_MSC_VER */


	/* copy fstat64() results to simulator memory */
	mem_bcopy(mem_fn, mem, Write, /*sbuf64*/regs->regs_R[5],
		  &ss_sbuf64, sizeof(struct ss_statbuf64));
      }
      break;
#endif


	case SS_SYS_pivot_root:
	{
		char new_root[MAXBUFSIZE], put_old[MAXBUFSIZE];

		/* copy arguments to the host memory */
		mem_strcpy(mem_fn, mem, Read, regs->regs_R[4], new_root);
		mem_strcpy(mem_fn, mem, Read, regs->regs_R[4], put_old);

		/*result*/regs->regs_R[2] = pivot_root(new_root, put_old);

		/* check for an error condition */
		if (regs->regs_R[2] != -1)
	  	  regs->regs_R[7] = 0;
		else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;

#if 0
    /* this syscall may be buggy */
	case SS_SYS_readahead:
	{
	  sqword_t offset64;

	  offset64 = ((sqword_t)regs->regs_R[5]<<32)|(sqword_t)regs->regs_R[6];

	  /*result*/regs->regs_R[2] = readahead(/*fd*/regs->regs_R[4], offset64,
	  	                                  /*count*/regs->regs_R[7]);

	  /* check for an error condition */
		if (regs->regs_R[2] != -1)
	  	  regs->regs_R[7] = 0;
		else
	    {
	      /* got an error, return details */
	      regs->regs_R[2] = errno;
	      regs->regs_R[7] = 1;
	    }
	}
	break;
	

case SS_SYS_gettid:
#ifdef _MSC_VER
      warn("syscall gettid() not yet implemented for MSC...");
	  regs->regs_R[7] = 0;
#else /*!_MSC_VER*/
      /* get the simulator process id */
      /*result*/regs->regs_R[2] = gettid();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
		regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#endif /*_MSC_VER*/
      break;
#endif
      
    case SS_SYS_getpagesize:
      /* get target pagesize */
      regs->regs_R[2] = /* was: getpagesize() */MD_PAGE_SIZE;

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
      break;
#if 0
    case SS_SYS_setitimer:
      /* FIXME: the sigvec system call is ignored */
      regs->regs_R[2] = regs->regs_R[7] = 0;
      warn("syscall: setitimer ignored");
      break;
#endif
    case SS_SYS_getdtablesize:
#if defined(_AIX)
      /* get descriptor table size */
      regs->regs_R[2] = getdtablesize();

      /* check for an error condition */
      if (regs->regs_R[2] != -1)
	regs->regs_R[7] = 0;
      else
	{
	  /* got an error, return details */
	  regs->regs_R[2] = errno;
	  regs->regs_R[7] = 1;
	}
#elif defined(__CYGWIN32__) || defined(ultrix) || defined(_MSC_VER)
      {
	/* no comparable system call found, try some reasonable defaults */
	warn("syscall: called getdtablesize()\n");
	regs->regs_R[2] = 16;
	regs->regs_R[7] = 0;
      }
#else
      {
	struct rlimit rl;

	/* get descriptor table size in rlimit structure */
	if (getrlimit(RLIMIT_NOFILE, &rl) != -1)
	  {
	    regs->regs_R[2] = rl.rlim_cur;
	    regs->regs_R[7] = 0;
	  }
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
#endif
      break;

  
    case SS_SYS_select:
#ifdef _MSC_VER
      warn("syscall select() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      {
	fd_set readfd, writefd, exceptfd;
	fd_set *readfdp, *writefdp, *exceptfdp;
	struct timeval timeout, *timeoutp;
	word_t param5;

	/* FIXME: swap words? */

	/* read the 5th parameter (timeout) from the stack */
	mem_bcopy(mem_fn, mem,
		  Read, regs->regs_R[29]+16, &param5, sizeof(word_t));

	/* copy read file descriptor set into host memory */
	if (/*readfd*/regs->regs_R[5] != 0)
	  {
	    mem_bcopy(mem_fn, mem, Read, /*readfd*/regs->regs_R[5],
		      &readfd, sizeof(fd_set));
	    readfdp = &readfd;
	  }
	else
	  readfdp = NULL;

	/* copy write file descriptor set into host memory */
	if (/*writefd*/regs->regs_R[6] != 0)
	  {
	    mem_bcopy(mem_fn, mem, Read, /*writefd*/regs->regs_R[6],
		      &writefd, sizeof(fd_set));
	    writefdp = &writefd;
	  }
	else
	  writefdp = NULL;

	/* copy exception file descriptor set into host memory */
	if (/*exceptfd*/regs->regs_R[7] != 0)
	  {
	    mem_bcopy(mem_fn, mem, Read, /*exceptfd*/regs->regs_R[7],
		      &exceptfd, sizeof(fd_set));
	    exceptfdp = &exceptfd;
	  }
	else
	  exceptfdp = NULL;

	/* copy timeout value into host memory */
	if (/*timeout*/param5 != 0)
	  {
	    mem_bcopy(mem_fn, mem, Read, /*timeout*/param5,
		      &timeout, sizeof(struct timeval));
	    timeoutp = &timeout;
	  }
	else
	  timeoutp = NULL;

#if defined(hpux) || defined(__hpux)
	/* select() on the specified file descriptors */
	/*result*/regs->regs_R[2] =
	  select(/*nfd*/regs->regs_R[4],
		 (int *)readfdp, (int *)writefdp, (int *)exceptfdp, timeoutp);
#else
	/* select() on the specified file descriptors */
	/*result*/regs->regs_R[2] =
	  select(/*nfd*/regs->regs_R[4],
		 readfdp, writefdp, exceptfdp, timeoutp);
#endif

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, return details */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* copy read file descriptor set to target memory */
	if (/*readfd*/regs->regs_R[5] != 0)
	  mem_bcopy(mem_fn, mem, Write, /*readfd*/regs->regs_R[5],
		    &readfd, sizeof(fd_set));

	/* copy write file descriptor set to target memory */
	if (/*writefd*/regs->regs_R[6] != 0)
	  mem_bcopy(mem_fn, mem, Write, /*writefd*/regs->regs_R[6],
		    &writefd, sizeof(fd_set));

	/* copy exception file descriptor set to target memory */
	if (/*exceptfd*/regs->regs_R[7] != 0)
	  mem_bcopy(mem_fn, mem, Write, /*exceptfd*/regs->regs_R[7],
		    &exceptfd, sizeof(fd_set));

	/* copy timeout value result to target memory */
	if (/* timeout */param5 != 0)
	  mem_bcopy(mem_fn, mem, Write, /*timeout*/param5,
		    &timeout, sizeof(struct timeval));
      }
#endif
      break;

    case SS_SYS_sigvec:
      /* FIXME: the sigvec system call is ignored */
      regs->regs_R[2] = regs->regs_R[7] = 0;
      warn("syscall: sigvec ignored");
      break;

    case SS_SYS_sigblock:
      /* FIXME: the sigblock system call is ignored */
      regs->regs_R[2] = regs->regs_R[7] = 0;
      warn("syscall: sigblock ignored");
      break;

    case SS_SYS_sigsetmask:
      /* FIXME: the sigsetmask system call is ignored */
      regs->regs_R[2] = regs->regs_R[7] = 0;
      warn("syscall: sigsetmask ignored");
      break;

#if 0
    case SS_SYS_sigstack:
      /* FIXME: this is broken... */
      /* do not make the system call; instead, modify (the stack
	 portion of) the simulator's main memory, ignore the 1st
	 argument (regs->regs_R[4]), as it relates to signal handling */
      if (regs->regs_R[5] != 0)
	{
	  (*maf)(Read, regs->regs_R[29]+28, (unsigned char *)&temp, 4);
	  (*maf)(Write, regs->regs_R[5], (unsigned char *)&temp, 4);
	}
      regs->regs_R[2] = regs->regs_R[7] = 0;
      break;
#endif



 
    case SS_SYS_writev:
#ifdef _MSC_VER
      warn("syscall writev() not yet implemented for MSC...");
      regs->regs_R[7] = 0;
#else /* !_MSC_VER */
      {
	int i;
	char *buf;
	struct iovec *iov;

	/* allocate host side I/O vectors */
	iov =
	  (struct iovec *)malloc(/*iovcnt*/regs->regs_R[6]
				 * sizeof(struct iovec));
	if (!iov)
	  fatal("out of virtual memory in SYS_writev");

	/* copy target side pointer data into host side vector */
	mem_bcopy(mem_fn, mem, Read, /*iov*/regs->regs_R[5],
		  iov, /*iovcnt*/regs->regs_R[6] * sizeof(struct iovec));

	/* copy target side I/O vector buffers to host memory */
	for (i=0; i < /*iovcnt*/regs->regs_R[6]; i++)
	  {
	    iov[i].iov_base = (char *)MD_SWAPW((unsigned)iov[i].iov_base);
	    iov[i].iov_len = MD_SWAPW(iov[i].iov_len);
	    if (iov[i].iov_base != NULL)
	      {
		buf = (char *)calloc(iov[i].iov_len, sizeof(char));
		if (!buf)
		  fatal("out of virtual memory in SYS_writev");
		mem_bcopy(mem_fn, mem, Read, (md_addr_t)iov[i].iov_base,
			  buf, iov[i].iov_len);
		iov[i].iov_base = buf;
	      }
	  }

	/* perform the vector'ed write */
	/*result*/regs->regs_R[2] =
	  writev(/*fd*/regs->regs_R[4], iov, /*iovcnt*/regs->regs_R[6]);

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate results */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* free all the allocated memory */
	for (i=0; i < /*iovcnt*/regs->regs_R[6]; i++)
	  {
	    if (iov[i].iov_base)
	      {
		free(iov[i].iov_base);
		iov[i].iov_base = NULL;
	      }
	  }
	free(iov);
      }
#endif /* !_MSC_VER */
      break;

    case SS_SYS_utimes:
      {
	char buf[MAXBUFSIZE];

	/* copy filename to host memory */
	mem_strcpy(mem_fn, mem, Read, /*fname*/regs->regs_R[4], buf);

	if (/*timeval*/regs->regs_R[5] == 0)
	  {
#if defined(hpux) || defined(__hpux) || defined(__i386__)
	    /* no utimes() in hpux, use utime() instead */
	    /*result*/regs->regs_R[2] = utime(buf, NULL);
#elif defined(_MSC_VER)
	    /* no utimes() in MSC, use utime() instead */
	    /*result*/regs->regs_R[2] = utime(buf, NULL);
#elif defined(__svr4__) || defined(__USLC__) || defined(unix) || defined(_AIX) || defined(__alpha)
	    /*result*/regs->regs_R[2] = utimes(buf, NULL);
#elif defined(__CYGWIN32__)
	    warn("syscall: called utimes()\n");
#else
#error No utimes() implementation!
#endif
	  }
	else
	  {
	    struct ss_timeval ss_tval[2];
#ifndef _MSC_VER
	    struct timeval tval[2];
#endif /* !_MSC_VER */

	    /* copy timeval structure to host memory */
	    mem_bcopy(mem_fn, mem, Read, /*timeout*/regs->regs_R[5],
		      ss_tval, 2*sizeof(struct ss_timeval));

#ifndef _MSC_VER
	    /* convert timeval structure to host format */
	    tval[0].tv_sec = MD_SWAPW(ss_tval[0].ss_tv_sec);
	    tval[0].tv_usec = MD_SWAPW(ss_tval[0].ss_tv_usec);
	    tval[1].tv_sec = MD_SWAPW(ss_tval[1].ss_tv_sec);
	    tval[1].tv_usec = MD_SWAPW(ss_tval[1].ss_tv_usec);
#endif /* !_MSC_VER */

#if defined(hpux) || defined(__hpux) || defined(__svr4__)
	    /* no utimes() in hpux, use utime() instead */
	    {
	      struct utimbuf ubuf;

	      ubuf.actime = tval[0].tv_sec;
	      ubuf.modtime = tval[1].tv_sec;

	      /* result */regs->regs_R[2] = utime(buf, &ubuf);
	    }
#elif defined(_MSC_VER)
	    /* no utimes() in MSC, use utime() instead */
	    {
	      struct _utimbuf ubuf;

	      ubuf.actime = ss_tval[0].ss_tv_sec;
	      ubuf.modtime = ss_tval[1].ss_tv_sec;

	      /* result */regs->regs_R[2] = utime(buf, &ubuf);
	    }
#elif defined(__USLC__) || defined(unix) || defined(_AIX) || defined(__alpha)
	    /* result */regs->regs_R[2] = utimes(buf, tval);
#elif defined(__CYGWIN32__)
	    warn("syscall: called utimes()\n");
#else
#error No utimes() implementation!
#endif
	  }

	/* check for an error condition */
	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    /* got an error, indicate results */
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }
      }
      break;


#if 0
    case SS_SYS_getdirentries:
      /* FIXME: this is currently broken due to incompatabilities in
	 disk directory formats */
      {
	unsigned int i;
	char *buf;
	int base;

	buf = (char *)calloc(/* nbytes */regs->regs_R[6] + 1, sizeof(char));
	if (!buf)
	  fatal("out of memory in SYS_getdirentries");

	/* copy in */
	for (i=0; i</* nbytes */regs->regs_R[6]; i++)
	  (*maf)(Read, /* buf */regs->regs_R[5]+i,
		 (unsigned char *)&buf[i], 1);
	(*maf)(Read, /* basep */regs->regs_R[7], (unsigned char *)&base, 4);

	/*cc*/regs->regs_R[2] =
	  getdirentries(/*fd*/regs->regs_R[4], buf,
			/*nbytes*/regs->regs_R[6], &base);

	if (regs->regs_R[2] != -1)
	  regs->regs_R[7] = 0;
	else
	  {
	    regs->regs_R[2] = errno;
	    regs->regs_R[7] = 1;
	  }

	/* copy out */
	for (i=0; i</* nbytes */regs->regs_R[6]; i++)
	  (*maf)(Write, /* buf */regs->regs_R[5]+i,
		 (unsigned char *)&buf[i], 1);
	(*maf)(Write, /* basep */regs->regs_R[7], (unsigned char *)&base, 4);

	free(buf);
      }
      break;
#endif


	case SS_SYS_rt_sigprocmask:
	{
		int i;
		/* mips has the same wide sigset_t with x86 */
		mips_sigset_t *set, *oset;

		set = (mips_sigset_t *)malloc(sizeof(mips_sigset_t));
		oset = (mips_sigset_t *)malloc(sizeof(mips_sigset_t));
		if ((!set)||(!oset))
		  fatal("out of virtual memory in SYS_rt_sigprocmask");
		regs->regs_R[7] = (unsigned long)regs->regs_R[7];
		
		if (regs->regs_R[7] != sizeof(mips_sigset_t))
		{
			/* error */
			regs->regs_R[2] = -EINVAL;
			regs->regs_R[7] = 1;
		}
		else
		{
		/* copy target side pointer data into host side memory */
		mem_bcopy(mem_fn, mem, Read, regs->regs_R[5],
		  set, sizeof(mips_sigset_t));

#if 0
		for (i=0; i<NSIG_WORDS; i++)
		{
		  regs->regs_R[2] = sigprocmask(/*how*/regs->regs_R[4], set->sig+i, oset->sig+i);
		  if (regs->regs_R[2] != -1)
		  	break;
		}
#else
		  regs->regs_R[2] = sigprocmask(/*how*/regs->regs_R[4], (sigset_t*)set, (sigset_t*)oset);
#endif
		/* copy host side memory into target side pointer data */
		mem_bcopy(mem_fn, mem, Write, regs->regs_R[6], 
		  oset, sizeof(mips_sigset_t));

		if (regs->regs_R[2] != -1)
		  regs->regs_R[7] = 0;
		else
		{
			regs->regs_R[2] = errno;
			regs->regs_R[7] = 1;
		}
		}

		/* free the host memory */
		free(set);
		free(oset);
	}
	break;
	case SS_SYS_ni_syscall1:
	case SS_SYS_ni_syscall2:
	case SS_SYS_ni_syscall3:
	case SS_SYS_ni_syscall4:
	case SS_SYS_ni_syscall5:
	case SS_SYS_ni_syscall6:
	case SS_SYS_ni_syscall7:
	case SS_SYS_ni_syscall8:
	case SS_SYS_ni_syscall9:
	case SS_SYS_ni_syscall10:
	case SS_SYS_ni_syscall11:
	case SS_SYS_ni_syscall12:
	case SS_SYS_ni_syscall13:
	case SS_SYS_ni_syscall14:
	case SS_SYS_ni_syscall15:
	case SS_SYS_ni_syscall16:
	case SS_SYS_ni_syscall17:
	case SS_SYS_ni_syscall18:
	{
		regs->regs_R[2] = -ENOSYS;
		regs->regs_R[7] = 1;
	}
	break;
	

	/* syscalls added for splash2 */
	
    case SS_MP_ACQUIRE_LOCK:
    {
	    int lock_id = regs->regs_R[4];

	    /* check if attempting to acquire another or same lock */
	    if (synchq[pid].synch_state == HOLDING_LOCK)
		  fatal ("pid %d attempting to get lock %d"
			 " but already has lock %d", pid, lock_id,
			 synchq[pid].synch_var);
	    else if (synchq[pid].synch_state != FREE)
		  fatal ("pid %d attempting to get lock %d"
			 " but already blocked on barrier or semaphore",
			 pid, lock_id);

	    /* check if at least one item in queue, which means lock
		   is already held by a thread */
	    if (locks[lock_id])
	    {
		  /* add ourselves to the end of the queue for this lock */
 		  synchq[pid].next = locks[lock_id]->next; /* point to head */
		  locks[lock_id]->next = &synchq[pid]; /* add to tail */
		  locks[lock_id] = &synchq[pid]; /* update pointer to tail */

		  /* set synch state and var */
		  synchq[pid].synch_state = WAITING_FOR_LOCK;
		  synchq[pid].synch_var = lock_id;
		  
		  /* mark as inactive until lock is acquired */
		  st->active = 0;

		  debug ("pid %d must wait for lock %d", pid, lock_id);
	    }
	    else
	    {
		  /* we will get the lock; put ourselves at head of queue */
 		  synchq[pid].next = &synchq[pid]; /* tail points to head */
		  locks[lock_id] = &synchq[pid]; /* update pointer to tail */

		  /* set synch state and var */
		  synchq[pid].synch_state = HOLDING_LOCK;
		  synchq[pid].synch_var = lock_id;

		  debug ("pid %d acquired lock %d", pid, lock_id);
	    }
	}
	break;
	  
    case SS_MP_RELEASE_LOCK:
    {
	    int lock_id = regs->regs_R[4];

	    /* sanity check: we must be at the head of the queue
	       (if lock is free, any thread can release) */
	    if (locks[lock_id] && locks[lock_id]->next->pid != pid)
	    {
		  fatal ("attempt to release lock held by another thread");
	    }
	      
	    /* set synch state of releasing thread*/
	    synchq[pid].synch_state = FREE;
	      
	    /* check if current lock holder is the only one in queue */
	    if (locks[lock_id]->next == locks[lock_id])
	    {
		  locks[lock_id] = NULL;	/* lock is now free */

		  debug ("pid %d released lock %d", pid, lock_id);
	    }
	    else /* at least one other thread waiting for lock */
	    {
		  /* advance head of queue */
		  locks[lock_id]->next = locks[lock_id]->next->next;
		  cpus[locks[lock_id]->next->pid].active = 1; /* mark as active */

		  /* set synch state and var of thread being given the lock */
		  locks[lock_id]->next->synch_state = HOLDING_LOCK;
		  locks[lock_id]->next->synch_var = lock_id;

		  debug ("pid %d given lock %d by pid %d",
			 locks[lock_id]->next->pid, lock_id, pid);
	    }
	}
	break;
     
    case SS_MP_INIT_LOCK:
    {
	    md_addr_t lock_ptr = regs->regs_R[4];
	    int	i;
	    int	lock_id = num_used_locks++;

	    if (num_used_locks > SANITY_LIMIT)
	      fatal ("do you _really_ need %d locks ??\n", num_used_locks);

	    if (num_used_locks > num_allocated_locks)
	    {
		  num_allocated_locks += LOCK_INCREMENT;
		  locks = (SynchQNode **) realloc (locks,
			       num_allocated_locks * sizeof (SynchQNode *));

		  /* initialize newly-allocated portion */
		  for (i = lock_id; i < num_allocated_locks; i++)
		  {
		      locks[i] = NULL;
		  }
	    }
	      
	    /* provide lock id to user program */
	    MEM_WRITE_WORD(mem, lock_ptr, lock_id);
	}
	break;
	
    case SS_MP_BARRIER:
	{
	    int barrier_id = regs->regs_R[4];

	    /* note that for simplicity, all threads, include the last
	       to arrive, are placed in the synch queue for the barrier */

	    /* check if at least one item already in queue */
	    if (barriers[barrier_id])
	    {
		/* add ourselves to the end of the queue for this barrier */
		synchq[pid].next = barriers[barrier_id]->next;/* point to head*/
		barriers[barrier_id]->next = &synchq[pid]; /* add to tail */
		barriers[barrier_id] = &synchq[pid]; /* update pointer to tail*/

		/* increment number of arrivals at barrier */
		++barrier_counts[barrier_id];
	    }
	    else
	    {
		/* first to arrive at this barrier */
		synchq[pid].next = &synchq[pid]; /* tail points to head */
		barriers[barrier_id] = &synchq[pid]; /* update pointer to tail */
		barrier_counts[barrier_id] = 1;	/* one arrival so far */
	    }

	    /* set synch state and var */
	    synchq[pid].synch_state = BLOCKED_ON_BARRIER;
	    synchq[pid].synch_var = barrier_id;

	    /* mark as inactive until we know that all have arrived */
	    st->active = 0;
	    debug ("pid %d arrived at barrier %d", pid, barrier_id);

	    /* now check if all have arrived at barrier */
	    if (regs->regs_R[5] == barrier_counts[barrier_id])
	    {
		SynchQNode	*p;

		/* traverse the queue and mark _all_ threads as active
		   (including the last arrival) */
		p = barriers[barrier_id]->next; /* get head of list */
		do
		{
		    cpus[p->pid].active = 1;

		    /* set synch state and var */
		    synchq[p->pid].synch_state = FREE;

		    debug ("pid %d leaving barrier %d", p->pid, barrier_id);
		    p = p->next;
		} while (p != barriers[barrier_id]->next); /* back at head? */

		/* reset count to zero */
		barrier_counts[barrier_id] = 0;

		/* last step is to set queue tail pointer to null */
		barriers[barrier_id] = NULL;
	    }
	}
	break;
	
    case SS_MP_INIT_BARRIER:
    {
	    md_addr_t barrier_ptr = regs->regs_R[4];
	    int	i;
	    int	barrier_id = num_used_barriers++;

	    if (num_used_barriers > SANITY_LIMIT)
		  fatal ("do you _really_ need %d barriers ??\n",num_used_barriers);

	    if (num_used_barriers > num_allocated_barriers)
	    {
		  num_allocated_barriers += BARRIER_INCREMENT;
		  barriers = (SynchQNode **) realloc (barriers,
			       num_allocated_barriers * sizeof (SynchQNode *));
		  barrier_counts = (int *) realloc (barrier_counts,
			       num_allocated_barriers * sizeof (int));

		  /* initialize newly-allocated portions */
		  for (i = barrier_id; i < num_allocated_barriers; i++)
		  {
		      barriers[i] = NULL;
		      barrier_counts[i] = 0;
		  }
	    }

	    /* provide barrier id to user program */
	    MEM_WRITE_WORD(mem, barrier_ptr, barrier_id);
	}
	break;
	
    case SS_MP_SEMA_WAIT:
    {
	    int sema_id = regs->regs_R[4];

	    /* if we decrement the count and it is negative, we must block */
	    if (--sema_counts[sema_id] < 0)
	    {
	      /* mark as inactive until matching signal unblocks it */
		  st->active = 0;
		  debug ("pid %d blocked on semaphore %d", pid, sema_id);

		  /* set synch state and var */
		  synchq[pid].synch_state = BLOCKED_ON_SEMAPHORE;
		  synchq[pid].synch_var = sema_id;

		  /* check if at least one item in queue, which means
		     at least one thread is already blocked on semaphore */
		  if (semaphores[sema_id])
		  {
		      /* add ourselves to end of queue for this semaphore */
		      synchq[pid].next = semaphores[sema_id]->next;
		      semaphores[sema_id]->next = &synchq[pid];
		      semaphores[sema_id] = &synchq[pid];
		  }
		  else
		  {
		      /* first to block; put ourselves at head of queue */
		      synchq[pid].next = &synchq[pid];
		      semaphores[sema_id] = &synchq[pid];
		  }
	    }
	    else
		  debug ("pid %d did not block on semaphore %d",pid, sema_id);
	}
	break;
	  
    case SS_MP_SEMA_SIGNAL:
    {
	    int sema_id = regs->regs_R[4];

	    /* increment count; if result is zero or negative,
	       there is at least one thread blocked, so unblock it */
	    if (++sema_counts[sema_id] <= 0)
	    {
		  /* sanity check: there must be at least one blocked thread
		     if the count is <= 0 */
		  if (semaphores[sema_id] == NULL)
		  {
		      fatal ("semaphore signal expected a blocked thread");
		  }

		  /* make thread at head of queue active */
		  cpus[semaphores[sema_id]->next->pid].active = 1;

		  /* set synch state of newly-reactivated thread */
		  semaphores[sema_id]->next->synch_state = FREE;
		  
		  debug ("pid %d unblocked on semaphore %d by pid %d",semaphores[sema_id]->next->pid, sema_id, pid);

		  /* check if queue has only one item */
		  if (semaphores[sema_id]->next == semaphores[sema_id])
		  {
		      semaphores[sema_id] = NULL; /* queue is now empty */
		  }
		  else /* at least one other thread was blocked */
		  {
		      /* advance head of queue */
		      semaphores[sema_id]->next =
			  semaphores[sema_id]->next->next;
		  }
	    }
	} 
	break;
     
    case SS_MP_INIT_SEMA:
    {
	    md_addr_t sema_ptr = regs->regs_R[4];
	    int	i;
	    int	sema_id = num_used_semaphores++;

	    if (num_used_semaphores > SANITY_LIMIT)
		  fatal ("do you _really_ need %d semaphores ??\n", num_used_semaphores);

	    if (num_used_semaphores > num_allocated_semaphores)
	    {
		  num_allocated_semaphores += SEMAPHORE_INCREMENT;
		  semaphores = (SynchQNode **)  realloc(semaphores,
			       num_allocated_semaphores*sizeof(SynchQNode *));
		  sema_counts = (int *)  realloc(sema_counts,
			       num_allocated_semaphores * sizeof (int));

		  /* initialize newly-allocated portions */
		  for (i = sema_id; i < num_allocated_semaphores; i++)
		  {
		      semaphores[i] = NULL;
		      sema_counts[i] = 0;
		  }
	    }

	    /* set initial semaphore count value */
	    sema_counts[sema_id] = regs->regs_R[5];

	    /* provide semaphore id to user program */
	    MEM_WRITE_WORD(mem, sema_ptr, sema_id);
	}
	break;
	
    case SS_MP_THREAD_ID:
	    regs->regs_R[7] = 0;
	    regs->regs_R[2] = pid;
	    break;
	  
    case SS_MP_CREATE_THREAD:
	{
		CreateNewProcess ();
		regs->regs_R[7] = 0;
		regs->regs_R[2] = 0;
	}
    break;
	  
    case SS_MP_EXIT_THREAD:
	{
	    if (pid == 0)
		panic("ERROR: main thread should not call exit_thread()\n");

	    ++num_terminated_processes;
	    st->active = 0; /* mark this one as inactive */
#if 0
	    if (num_terminated_processes == num_created_processes)
	    {
		/* exit jumps to the target set in main() */
		longjmp(sim_exit_buf, /* exitcode + fudge */regs->regs_R[4]+1);
	    }
#endif
	    /* else we must wait until last thread finishes
	       (this one has been marked inactive, so when we return
	        to the main simulation loop, no further instructions
	        will be fetched for this thread) */
	}
	break;

    case SS_MP_MALLOC:
	{
	  unsigned int org_size, round_size;

	  /* round the new heap pointer to the its page boundary */
	  //addr = ROUND_UP(/*base*/regs->regs_R[4], MD_PAGE_SIZE);
	  assert(org_size < 0x7fffffff);
	  round_size = org_size = regs->regs_R[4];
	  /*while (tmp) {
		round_size = tmp;
		tmp &= tmp - 1;
		}
	  if (round_size != org_size)
		round_size <<= 1;*/

	  round_size = (org_size + 7) & ~7;
	  //if (round_size != 0)
	  //st->ld_brk_point = (st->ld_brk_point + (round_size - 1)) & ~(round_size - 1);
	  
	  /* check whether heap area has merged with stack area */
	  if (st->ld_brk_point + round_size < (md_addr_t)regs->regs_R[29])
	  {
		printf("malloc:ld_brk_point addr is 0x%08x, cpuid=%d, org_size=0x%x, round_size=0x%x\n",
			   st->ld_brk_point, st->cpuid, org_size, round_size);
		regs->regs_R[2] = st->ld_brk_point;
		regs->regs_R[7] = 0;
		if (round_size)
		  st->ld_brk_point += round_size;
		else {
		  st->ld_brk_point += 8;
		  printf("abnormal malloc:ld_brk_point addr is 0x%08x, cpuid=%d, org_size=0x%x, round_size=0x%x\n",
			  st->ld_brk_point, st->cpuid, org_size, round_size);
		}
	  }
	  else if (st->ld_brk_point + round_size >= (md_addr_t)regs->regs_R[29])
	  {
		printf("malloc:overflow:sp=0x%08x, org_size=0x%x, round_size=0x%x, cpuid=%d\n",
			regs->regs_R[29], org_size, round_size, st->cpuid);
		/* out of address space, indicate error */
		regs->regs_R[2] = ENOMEM;
		regs->regs_R[7] = 1;
	  }
	  else
	  {
		printf("malloc:heap-shrink:ld_brk_point=0x%08x, org_size=0x%x, round_size=0x%x, cpuid=%d\n",
			st->ld_brk_point, org_size, round_size, st->cpuid);
		regs->regs_R[2] = st->ld_brk_point;
		regs->regs_R[7] = 0;
	  }
	}
	break;	

    default:
      //panic("invalid/unimplemented system call encountered, code %d", syscode);
      warn("invalid/unimplemented system call encountered, code %d", syscode);
      regs->regs_R[2] = -ENOSYS;
      regs->regs_R[7] = 1;
    }

#if 0 //def DEBUG
    if (regs->regs_R[7] != 0 ) {
      warn("syscall %d failed\n",syscode);
      warn("a0=%x,a1=%x,a2=%x,ret=%x\n",
	  regs->regs_R[4],regs->regs_R[5],regs->regs_R[6],regs->regs_R[2]);
    }
#endif
#endif /* MD_CROSS_ENDIAN */

}
