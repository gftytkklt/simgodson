/*
 * mips.h - SimpleScalar MIPS ISA (mips) definitions
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
 * $Id: mips.h,v 1.2 2006/10/12 06:43:02 cvsuser Exp $
 *
 * $Log: mips.h,v $
 * Revision 1.2  2006/10/12 06:43:02  cvsuser
 * originally, stacks of all processes are allocated on the LAST cpu and
 * all heaps reside in the FIRST cpu, which is not reasonable. These
 * modifications help heap and stack allocations happen in local CPU memory.
 * A new "malloc" system call is added to replace the glibc malloc to support
 * the new allocation scheme, because the glibc malloc could not switch malloc
 * starting point in the current simulator.
 *
 * Revision 1.1.1.1  2006/09/08 09:21:43  cvsuser
 * Godson-3 simulator
 *
 * Revision 1.4  2005/02/06 03:06:25  fxzhang
 * likely branch
 *
 * Revision 1.3  2005/02/01 04:37:31  fxzhang
 * tmp
 *
 * Revision 1.2  2005/01/28 10:05:17  fxzhang
 * runnable,without mem hierarchy
 *
 * Revision 1.1  2005/01/27 03:18:24  fxzhang
 * create godson2 cvs
 *
 * Revision 1.1.1.1  2004/12/05 14:36:32  fxzhang
 * initial import of ss-mips
 *
 * Revision 1.3  2004/11/17 05:04:58  fxzhang
 * dump checkpoint support start running
 *
 * Revision 1.2  2004/08/03 09:50:06  fxzhang
 * merge back changes on AMD64
 *
 * Revision 1.2  2004/07/30 02:48:22  fxzhang
 * fix stat64/lstat64
 *
 * Revision 1.2  2004/07/15 07:31:03  fxzhang
 * fix syscall on x86-64
 *
 * Revision 1.1.1.1  2004/07/15 01:26:34  fxzhang
 * import
 *
 * Revision 1.1.1.1  2004/07/10 16:46:48  fxzhang
 * initial
 *
 * Revision 1.1.1.1  2000/05/26 15:21:54  taustin
 * SimpleScalar Tool Set
 *
 *
 * Revision 1.9  1999/12/31 19:03:38  taustin
 * quad_t naming conflicts removed
 *
 * Revision 1.8  1999/12/13 19:00:56  taustin
 * cross endian execution support added
 *
 * Revision 1.7  1998/08/31 17:13:14  taustin
 * added register checksuming routines
 *
 * Revision 1.6  1998/08/27 17:05:36  taustin
 * added target interface support
 * moved target-dependent definitions to target files
 * added support for register and memory contexts
 * LWL/LWR/SWL/SWR semantics fixed in ss.def, these instruction now
 *       appear to work correctly on big- and little-endian machines, this
 *       fixes all previous problems with IJPEG failing during functional
 *       simulation
 *
 *
 */

#ifndef MIPS_H
#define MIPS_H

#include <stdio.h>

#include "host.h"
#include "misc.h"
#include "endian.h"

/*
 * This file contains various definitions needed to decode, disassemble, and
 * execute MIPS instructions.
 */

/* build for MIPS target */
#define TARGET_MIPS

#ifndef TARGET_MIPS_BIG
#ifndef TARGET_MIPS_LITTLE
/* no cross-endian support, default to host endian */
#ifdef BYTES_BIG_ENDIAN
#define TARGET_MIPS_BIG
#else
#define TARGET_MIPS_LITTLE
#endif
#endif /* TARGET_MIPS_LITTLE */
#endif /* TARGET_MIPS_BIG */

/* probe cross-endian execution */
#if defined(BYTES_BIG_ENDIAN) && defined(TARGET_MIPS_LITTLE)
#define MD_CROSS_ENDIAN
#endif
#if defined(BYTES_LITTLE_ENDIAN) && defined(TARGET_MIPS_BIG)
#define MD_CROSS_ENDIAN
#endif

/* not applicable/available, usable in most definition contexts */
#define NA		0

/*
 * target-dependent type definitions
 */

/* define MD_QWORD_ADDRS if the target requires 64-bit (qword) addresses */
#undef MD_QWORD_ADDRS

/* address type definition */
typedef word_t md_addr_t;


/*
 * target-dependent memory module configuration
 */

/* physical memory page size (must be a power-of-two) */
#if 1
#define MD_PAGE_SIZE		4096
#define MD_LOG_PAGE_SIZE	12
#else
#define MD_PAGE_SIZE		16384
#define MD_LOG_PAGE_SIZE	14
#endif


#define MD_LOG_MEM_SIZE     31


/*
 * target-dependent instruction faults
 */

enum md_fault_type {
  md_fault_none = 0,		/* no fault */
  md_fault_access,		/* storage access fault */
  md_fault_alignment,		/* storage alignment fault */
  md_fault_overflow,		/* signed arithmetic overflow fault */
  md_fault_div0,		/* division by zero fault */
  md_fault_break,		/* BREAK instruction fault */
  md_fault_unimpl,		/* unimplemented instruction fault */
  md_fault_internal		/* internal S/W fault */
};


/*
 * target-dependent register file definitions, used by regs.[hc]
 */

/* number of integer registers */
#define MD_NUM_IREGS		32

/* number of floating point registers */
#define MD_NUM_FREGS		32

/* number of control registers */
#define MD_NUM_CREGS		3

/* total number of registers, excluding PC and NPC */
#define MD_TOTAL_REGS							\
  (/*int*/32 + /*fp*/32 + /*misc*/3 + /*tmp*/1 + /*mem*/1 + /*ctrl*/1)

/* general purpose (integer) register file entry type */
typedef sword_t md_gpr_t[MD_NUM_IREGS];

/* floating point register file entry type */
typedef union {
  sword_t  l[MD_NUM_FREGS];	/* integer word view */
  sfloat_t f[MD_NUM_FREGS];	/* single-precision floating point view */
  dfloat_t d[MD_NUM_FREGS/2]; /* double-prediction floating point view */
} md_fpr_t;

/* control register file contents */
typedef struct {
  sword_t hi, lo;	/* multiplier HI/LO result registers */
  int fcc;			/* floating point condition codes */
} md_ctrl_t;

/* well known registers */
enum md_reg_names {
  MD_REG_ZERO = 0,	/* zero register */
  MD_REG_GP = 28,	/* global data section pointer */
  MD_REG_SP = 29,	/* stack pointer */
  MD_REG_FP = 30	/* frame pointer */
};


/*
 * target-dependent instruction format definition
 */

/* instruction formats */
typedef word_t md_inst_t;  

/* preferred nop instruction definition */
extern md_inst_t MD_NOP_INST;

/* target swap support */
#ifdef MD_CROSS_ENDIAN

#define MD_SWAPH(X)		SWAP_HALF(X)
#define MD_SWAPW(X)		SWAP_WORD(X)
#define MD_SWAPQ(X)		SWAP_QWORD(X)
#define MD_SWAPI(X)		(X).b = SWAP_WORD((X).b)

#else /* !MD_CROSS_ENDIAN */

#define MD_SWAPH(X)		(X)
#define MD_SWAPW(X)		(X)
#define MD_SWAPQ(X)		(X)
#define MD_SWAPD(X)		(X)
#define MD_SWAPI(X)		(X)

#endif

/* fetch an instruction */
#define MD_FETCH_INST(INST,MEM, PC)                                    \
  { (INST) = MEM_READ_WORD((MEM),(PC)); }


/*
 * target-dependent loader module configuration
 */

/* virtual memory segment limits */
#if 1
#define MD_TEXT_BASE		0x00400000
#else
#define MD_TEXT_BASE		0x04000000
#endif

#define MD_DATA_BASE		0x02000000
#define MD_STACK_BASE 		((unsigned long)(1<<MD_LOG_MEM_SIZE)/total_cpus-0x20000)  /* 0x07ffffff */

#define MMAP_START_ADDR 	0x04000000
  
/* maximum size of argc+argv+envp environment */
#define MD_MAX_ENVIRON		16384


/*
 * mips.def specific definitions
 */

/* inst -> enum md_opcode mapping, use this macro to decode insts */
#define MD_TOP_OP(INST)		(((INST) >> 26) & 0x3f)
#define MD_SET_OPCODE(OP, INST)						\
  { OP = md_mask2op[MD_TOP_OP(INST)];					\
    while (md_opmask[OP])						\
      OP = md_mask2op[((INST >> md_opshift[OP]) & md_opmask[OP])	\
		      + md_opoffset[OP]]; }

/* largest opcode field value (currently upper 8-bit are used for pre/post-
    incr/decr operation specifiers */
#define MD_MAX_MASK		2048

/* internal decoder state */
extern unsigned int md_opoffset[];
extern unsigned int md_opmask[];
extern unsigned int md_opshift[];

/* global opcode names, these are returned by the decoder (MD_OP_ENUM()) */
enum md_opcode {
  OP_NA = 0,	/* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3) OP,
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT) OP,
#define CONNECT(OP)
#include "mips.def"
  OP_MAX	/* number of opcodes + NA */
};

extern enum md_opcode md_mask2op[];
/* enum md_opcode -> description string */
#define MD_OP_NAME(OP)		(md_op2name[OP])
extern char *md_op2name[];

/* enum md_opcode -> opcode operand format, used by disassembler */
#define MD_OP_FORMAT(OP)	(md_op2format[OP])
extern char *md_op2format[];

/* function unit classes, update md_fu2name if you update this definition */
enum md_fu_class {
  FUClass_NA = 0,	/* inst does not use a functional unit */
  IntALU,		/* integer ALU */
  IntMULT,		/* integer multiplier */
  IntDIV,		/* integer divider */
  FloatADD,		/* floating point adder/subtractor */
  FloatCMP,		/* floating point comparator */
  FloatCVT,		/* floating point<->integer converter */
  FloatMULT,		/* floating point multiplier */
  FloatDIV,		/* floating point divider */
  FloatSQRT,		/* floating point square root */
  RdPort,		/* memory read port */
  WrPort,		/* memory write port */
  IntBR,		/* integer branch */
  FpBR,		    /* floating-point branch */
  NUM_FU_CLASSES	/* total functional unit classes */
};

/* enum md_opcode -> enum md_fu_class, used by performance simulators */
#define MD_OP_FUCLASS(OP)	(md_op2fu[OP])
extern enum md_fu_class md_op2fu[];

/* enum md_fu_class -> description string */
#define MD_FU_NAME(FU)		(md_fu2name[FU])
extern char *md_fu2name[];

/* instruction flags */
#define F_ICOMP		0x00000001	/* integer computation */
#define F_FCOMP		0x00000002	/* FP computation */
#define F_CTRL		0x00000004	/* control inst */
#define F_UNCOND	0x00000008	/*   unconditional change */
#define F_COND		0x00000010	/*   conditional change */
#define F_MEM		0x00000020	/* memory access inst */
#define F_LOAD		0x00000040	/*   load inst */
#define F_STORE		0x00000080	/*   store inst */
#define F_DISP		0x00000100	/*   displaced (R+C) addr mode */
#define F_RR		0x00000200	/*   R+R addr mode */
#define F_DIRECT	0x00000400	/*   direct addressing mode */
#define F_TRAP		0x00000800	/* traping inst */
#define F_LONGLAT	0x00001000	/* long latency inst (for sched) */
#define F_DIRJMP	0x00002000	/* direct jump */
#define F_INDIRJMP	0x00004000	/* indirect jump */
#define F_CALL		0x00008000	/* function call */
#define F_FPCOND	0x00010000	/* FP conditional branch */
#define F_IMM		0x00020000	/* instruction has immediate operand */
#define F_CTRL_LIKELY 0x00040000	/* likely branch inst */

/* enum md_opcode -> opcode flags, used by simulators */
#define MD_OP_FLAGS(OP)		(md_op2flags[OP])
extern unsigned int md_op2flags[];

/* Allows a resource to be one of several types */
enum resource_type { Cache, TLB, Bus, Memory };

/* integer register specifiers */
#undef  RS	/* defined in /usr/include/sys/syscall.h on HPUX boxes */
#define RS		((inst >> 21) & 0x1f)			/* reg source #1 */
#define RT		((inst >> 16) & 0x1f)		/* reg source #2 */
#define RD		((inst >> 11) & 0x1f)		/* reg dest */

/* returns shift amount field value */
#define SHAMT		((inst >> 6) & 0x1f)

/* floating point register field synonyms */
#define FS		RD
#define FT		RT
#define FD		((inst >> 6) & 0x1f)

/* returns 16-bit signed immediate field value */
#define IMM		((int)((/* signed */short)(inst & 0xffff)))

/* returns 16-bit unsigned immediate field value */
#define UIMM		(inst & 0xffff)

/* returns 26-bit unsigned absolute jump target field value */
#define TARG		(inst & 0x3ffffff)

/* returns break code immediate field value */
#define BCODE		(inst & 0xfffff)

/* load/store 16-bit signed offset field value, synonym for imm field */
#define OFS		IMM		/* alias to IMM */

/* load/store base register specifier, synonym for RS field */
#define BS		RS		/* alias to rs */

/* largest signed integer */
#define MAXINT_VAL	0x7fffffff

/* check for overflow in X+Y, both signed */
#define OVER(X,Y)							\
  ((((X) > 0) && ((Y) > 0) && (MAXINT_VAL - (X) < (Y)))			\
   || (((X) < 0) && ((Y) < 0) && (-MAXINT_VAL - (X) > (Y))))

/* check for underflow in X-Y, both signed */
#define UNDER(X,Y)							\
  ((((X) > 0) && ((Y) < 0) && (MAXINT_VAL + (Y) < (X)))			\
   || (((X) < 0) && ((Y) > 0) && (-MAXINT_VAL + (Y) > (X))))

/* default target PC handling */
#ifndef SET_TPC
extern md_addr_t target_PC;
#define SET_TPC(PC)	{ target_PC = (PC); }	
#endif /* SET_TPC */

#define NOP  0

#ifdef BYTES_BIG_ENDIAN
/* lwl/swl defs */
#define WL_SIZE(ADDR)		((ADDR) & 0x03)
#define WL_BASE(ADDR)		((ADDR) & ~0x03)
#define WL_PROT_MASK(ADDR)	(md_lr_masks[4-WL_SIZE(ADDR)])
#define WL_PROT_MASK1(ADDR)	(md_lr_masks[WL_SIZE(ADDR)])
#define WL_PROT_MASK2(ADDR)	(md_lr_masks[4-WL_SIZE(ADDR)])

/* lwr/swr defs */
#define WR_SIZE(ADDR)		(((ADDR) & 0x03)+1)
#define WR_BASE(ADDR)		((ADDR) & ~0x03)
#define WR_PROT_MASK(ADDR)	(~(md_lr_masks[WR_SIZE(ADDR)]))
#define WR_PROT_MASK1(ADDR)	((md_lr_masks[WR_SIZE(ADDR)]))
#define WR_PROT_MASK2(ADDR)	(md_lr_masks[4-WR_SIZE(ADDR)])
#else /* BYTES_LITTLE_ENDIAN */
/* lwl/swl defs */
#define WL_SIZE(ADDR)		(4-((ADDR) & 0x03))
#define WL_BASE(ADDR)		((ADDR) & ~0x03)
#define WL_PROT_MASK(ADDR)	(md_lr_masks[4-WL_SIZE(ADDR)])
#define WL_PROT_MASK1(ADDR)	(md_lr_masks[WL_SIZE(ADDR)])
#define WL_PROT_MASK2(ADDR)	(md_lr_masks[4-WL_SIZE(ADDR)])

/* lwr/swr defs */
#define WR_SIZE(ADDR)		(((ADDR) & 0x03)+1)
#define WR_BASE(ADDR)		((ADDR) & ~0x03)
#define WR_PROT_MASK(ADDR)	(~(md_lr_masks[WR_SIZE(ADDR)]))
#define WR_PROT_MASK1(ADDR)	((md_lr_masks[WR_SIZE(ADDR)]))
#define WR_PROT_MASK2(ADDR)	(md_lr_masks[4-WR_SIZE(ADDR)])
#endif
  
/* mask table used to speed up LWL/LWR implementation */
extern word_t md_lr_masks[];

/*
 * various other helper macros/functions
 */

/* non-zero if system call is an exit() */
#define	SS_SYS_exit			4001
#define MD_EXIT_SYSCALL(REGS)		((REGS)->regs_R[2] == SS_SYS_exit)

/* non-zero if system call is a write to stdout/stderr */
#define	SS_SYS_write		4004
#define MD_OUTPUT_SYSCALL(REGS)						\
  ((REGS)->regs_R[2] == SS_SYS_write					\
   && ((REGS)->regs_R[4] == /* stdout */1				\
       || (REGS)->regs_R[4] == /* stderr */2))

/* returns stream of an output system call, translated to host */
#define MD_STREAM_FILENO(REGS)		((REGS)->regs_R[4])

/* returns non-zero if instruction is a function call */
#define MD_IS_CALL(OP)							\
  ((MD_OP_FLAGS(OP) & (F_CTRL|F_CALL)) == (F_CTRL|F_CALL))

/* returns non-zero if instruction is a function return */
#define MD_IS_RETURN(OP)		((OP) == JR && (RS) == 31)

/* returns non-zero if instruction is an indirect jump */
#define MD_IS_INDIR(OP)			((OP) == JR || (OP) == JALR)

/* addressing mode probe, enums and strings */
enum md_amode_type {
  md_amode_imm,		/* immediate addressing mode */
  md_amode_gp,		/* global data access through global pointer */
  md_amode_sp,		/* stack access through stack pointer */
  md_amode_fp,		/* stack access through frame pointer */
  md_amode_disp,	/* (reg + const) addressing */
  md_amode_rr,		/* (reg + reg) addressing */
  md_amode_NUM
};
extern char *md_amode_str[md_amode_NUM];

/* addressing mode pre-probe FSM, must see all instructions */
#define MD_AMODE_PREPROBE(OP, FSM)					\
  { if ((OP) == LUI) (FSM) = (RT); }

/* compute addressing mode, only for loads/stores */
#define MD_AMODE_PROBE(AM, OP, FSM)					\
  {									\
    if (MD_OP_FLAGS(OP) & F_DISP)					\
      {									\
	if ((BS) == (FSM))						\
	  (AM) = md_amode_imm;						\
	else if ((BS) == MD_REG_GP)					\
	  (AM) = md_amode_gp;						\
	else if ((BS) == MD_REG_SP)					\
	  (AM) = md_amode_sp;						\
	else if ((BS) == MD_REG_FP) /* && bind_to_seg(addr) == seg_stack */\
	  (AM) = md_amode_fp;						\
	else								\
	  (AM) = md_amode_disp;						\
      }									\
    else if (MD_OP_FLAGS(OP) & F_RR)					\
      (AM) = md_amode_rr;						\
    else								\
      panic("cannot decode addressing mode");				\
  }

/* addressing mode pre-probe FSM, after all loads and stores */
#define MD_AMODE_POSTPROBE(FSM)						\
  { (FSM) = MD_REG_ZERO; }


/*
 * EIO package configuration/macros
 */

/* expected EIO file format */
#define MD_EIO_FILE_FORMAT		EIO_MIPS_FORMAT

#define MD_MISC_REGS_TO_EXO(REGS)					\
  exo_new(ec_list,							\
	  /*icnt*/exo_new(ec_integer, (exo_integer_t)sim_pop_insn),	\
	  /*PC*/exo_new(ec_address, (exo_integer_t)(REGS)->regs_PC),	\
	  /*NPC*/exo_new(ec_address, (exo_integer_t)(REGS)->regs_NPC),	\
	  /*HI*/exo_new(ec_integer, (exo_integer_t)(REGS)->regs_C.hi),	\
	  /*LO*/exo_new(ec_integer, (exo_integer_t)(REGS)->regs_C.lo),	\
	  /*FCC*/exo_new(ec_integer, (exo_integer_t)(REGS)->regs_C.fcc),\
	  NULL)

#define MD_IREG_TO_EXO(REGS, IDX)					\
  exo_new(ec_address, (exo_integer_t)(REGS)->regs_R[IDX])

#define MD_FREG_TO_EXO(REGS, IDX)					\
  exo_new(ec_address, (exo_integer_t)(REGS)->regs_F.l[IDX])

#define MD_EXO_TO_MISC_REGS(EXO, ICNT, REGS)				\
  /* check EXO format for errors... */					\
  if (!exo								\
      || exo->ec != ec_list						\
      || !exo->as_list.head						\
      || exo->as_list.head->ec != ec_integer				\
      || !exo->as_list.head->next					\
      || exo->as_list.head->next->ec != ec_address			\
      || !exo->as_list.head->next->next					\
      || exo->as_list.head->next->next->ec != ec_address		\
      || !exo->as_list.head->next->next->next				\
      || exo->as_list.head->next->next->next->ec != ec_integer		\
      || !exo->as_list.head->next->next->next->next			\
      || exo->as_list.head->next->next->next->next->ec != ec_integer	\
      || !exo->as_list.head->next->next->next->next->next		\
      || exo->as_list.head->next->next->next->next->next->ec != ec_integer\
      || exo->as_list.head->next->next->next->next->next->next != NULL)	\
    fatal("could not read EIO misc regs");				\
  (ICNT) = (counter_t)exo->as_list.head->as_integer.val;		\
  (REGS)->regs_PC = (md_addr_t)exo->as_list.head->next->as_address.val;	\
  (REGS)->regs_NPC =							\
    (md_addr_t)exo->as_list.head->next->next->as_address.val;		\
  (REGS)->regs_C.hi =							\
    (word_t)exo->as_list.head->next->next->next->as_integer.val;	\
  (REGS)->regs_C.lo =							\
    (word_t)exo->as_list.head->next->next->next->next->as_integer.val;	\
  (REGS)->regs_C.fcc =							\
    (int)exo->as_list.head->next->next->next->next->next->as_integer.val;

#define MD_EXO_TO_IREG(EXO, REGS, IDX)					\
  ((REGS)->regs_R[IDX] = (word_t)(EXO)->as_integer.val)

#define MD_EXO_TO_FREG(EXO, REGS, IDX)					\
  ((REGS)->regs_F.l[IDX] = (word_t)(EXO)->as_integer.val)

#define MD_EXO_CMP_IREG(EXO, REGS, IDX)					\
  ((REGS)->regs_R[IDX] != (sword_t)(EXO)->as_integer.val)

#define MD_FIRST_IN_REG			2
#define MD_LAST_IN_REG			7

#define MD_FIRST_OUT_REG		2
#define MD_LAST_OUT_REG			7


/*
 * configure the EXO package
 */

/* EXO pointer class */
typedef qword_t exo_address_t;

/* EXO integer class, 64-bit encoding */
typedef qword_t exo_integer_t;

/* EXO floating point class, 64-bit encoding */
typedef double exo_float_t;


/*
 * configure the stats package
 */

/* counter stats */
#ifdef HOST_HAS_QWORD
#define stat_reg_counter		stat_reg_sqword
#define sc_counter	 	    	sc_sqword
#define for_counter	    		for_sqword
#else /* !HOST_HAS_QWORD */
#define stat_reg_counter		stat_reg_double
#define sc_counter		    	sc_double
#define for_counter		    	for_double
#endif /* HOST_HAS_QWORD */

/* address stats */
#define stat_reg_addr			stat_reg_uint


/*
 * configure the DLite! debugger
 */

/* register bank specifier */
enum md_reg_type {
  rt_gpr,		/* general purpose register */
  rt_lpr,		/* integer-precision floating pointer register */
  rt_fpr,		/* single-precision floating pointer register */
  rt_dpr,		/* double-precision floating pointer register */
  rt_ctrl,		/* control register */
  rt_PC,		/* program counter */
  rt_NPC,		/* next program counter */
  rt_NUM
};

/* register name specifier */
struct md_reg_names_t {
  char *str;			/* register name */
  enum md_reg_type file;	/* register file */
  int reg;			/* register index */
};

/* symbolic register names, parser is case-insensitive */
extern struct md_reg_names_t md_reg_names[];

/* returns a register name string */
char *md_reg_name(enum md_reg_type rt, int reg);

/* default register accessor object */
struct eval_value_t;
struct regs_t;
char *						/* err str, NULL for no err */
md_reg_obj(struct regs_t *regs,			/* registers to access */
	   int is_write,			/* access type */
	   enum md_reg_type rt,			/* reg bank to probe */
	   int reg,				/* register number */
	   struct eval_value_t *val);		/* input, output */

/* print integer REG(S) to STREAM */
void md_print_ireg(md_gpr_t regs, int reg, FILE *stream);
void md_print_iregs(md_gpr_t regs, FILE *stream);

/* print floating point REG(S) to STREAM */
void md_print_fpreg(md_fpr_t regs, int reg, FILE *stream);
void md_print_fpregs(md_fpr_t regs, FILE *stream);

/* print control REG(S) to STREAM */
void md_print_creg(md_ctrl_t regs, int reg, FILE *stream);
void md_print_cregs(md_ctrl_t regs, FILE *stream);

/* compute CRC of all registers */
word_t md_crc_regs(struct regs_t *regs);

/* xor checksum registers */
word_t md_xor_regs(struct regs_t *regs);


/*
 * configure sim-outorder specifics
 */

/* primitive operation used to compute addresses within pipeline */
#define MD_AGEN_OP		ADD

/* NOP operation when injected into the pipeline */
#define MD_NOP_OP		NOP

/* non-zero for a valid address, used to determine if speculative accesses
   should access the DL1 data cache */
#define MD_VALID_ADDR(st,ADDR)						\
  (((ADDR) >= st->ld_text_base && (ADDR) < (st->ld_text_base + st->ld_text_size))	\
   || ((ADDR) >= st->ld_data_base && (ADDR) < st->ld_stack_base))


/*
 * configure branch predictors
 */

/* shift used to ignore branch address least significant bits, usually
   log2(sizeof(md_inst_t)) */
#define MD_BR_SHIFT		2	


/*
 * target-dependent routines
 */

/* intialize the inst decoder, this function builds the ISA decode tables */
void md_init_decoder(void);

/* disassemble an instruction */
void
md_print_insn(md_inst_t inst,		/* instruction to disassemble */
	      md_addr_t pc,		/* addr of inst, used for PC-rels */
	      FILE *stream);		/* output stream */


/* this won't be included in math.h unless C99 is used */
extern double round(double);
extern float roundf(float);
extern double trunc(double);
extern float truncf(float);

/* flag of the delay slot */
extern int is_jump;
                                                                                
/* flag of branch likely delay slot annulling*/
extern int is_annulled;

/* mmap,handle mmap */
extern md_addr_t mmap_base;
                                                                                
/* mmap segment size in bytes */
extern unsigned int mmap_size;

/* Simulated machine frequency */
extern unsigned int mips_cpu_freq;

#endif /* MIPS_H */




