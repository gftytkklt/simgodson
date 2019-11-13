/*
 * mpsyscall.h - proxy system call handler interfaces; multiprocessing
 *
 * This file is used in conjunction with the SimpleScalar tool suite
 * originally written by Todd M. Austin for the Multiscalar Research Project
 * at the University of Wisconsin-Madison.
 *
 * The file was created by Naraig Manjikian at Queen's University,
 * Kingston, Ontario, Canada.
 *
 * Copyright (C) 2000 by Naraig Manjikian
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
 */

/*
 * >>>> IMPORTANT NOTE:
 *
 * This file is included by 'syscall.h' for use in C code,
 * _and_ it is also included in an assembly-language file
 * so that the constant values used for system calls are
 * consistent. It is recommended that this file only contain
 * #define directives for the preprocessor.
 */

#ifndef MPSYSCALL_H
#define MPSYSCALL_H

/* codes for multiprocessor support */

#define SS_MP_ACQUIRE_LOCK	300
#define SS_MP_RELEASE_LOCK	301
#define SS_MP_INIT_LOCK		302
#define SS_MP_BARRIER		303
#define SS_MP_INIT_BARRIER	304
#define SS_MP_THREAD_ID		305
#define SS_MP_CREATE_THREAD	306
#define SS_MP_EXIT_THREAD	307
#define	SS_MP_SEMA_WAIT		308
#define SS_MP_SEMA_SIGNAL	309
#define SS_MP_INIT_SEMA		310
#define SS_MP_MALLOC		311

#endif /* MPSYSCALL_H */
