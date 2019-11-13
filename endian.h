/*
 * endian.h - host endian probe interfaces
 * 
 * This file is part of the Alpha simulator tool suite written by
 * Raj Desikan as part of the Bullseye project.
 * It has been written by extending the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *  
 * 
 * Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
 *
 * Copyright (C) 1999 by Raj Desikan
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
 *
 *
 */

#ifndef ENDIAN_H
#define ENDIAN_H

/* data swapping functions, from big/little to little/big endian format */
#define SWAP_HALF(X)							\
  (((((half_t)(X)) & 0xff) << 8) | ((((half_t)(X)) & 0xff00) >> 8))
#define SWAP_WORD(X)	(((word_t)(X) << 24) |				\
			 (((word_t)(X) << 8)  & 0x00ff0000) |		\
			 (((word_t)(X) >> 8)  & 0x0000ff00) |		\
			 (((word_t)(X) >> 24) & 0x000000ff))

#define SWAP_QWORD(X)	(((quad_t)(X) << 56) |				\
			 (((quad_t)(X) << 40) & ULL(0x00ff000000000000)) |\
			 (((quad_t)(X) << 24) & ULL(0x0000ff0000000000)) |\
			 (((quad_t)(X) << 8)  & ULL(0x000000ff00000000)) |\
			 (((quad_t)(X) >> 8)  & ULL(0x00000000ff000000)) |\
			 (((quad_t)(X) >> 24) & ULL(0x0000000000ff0000)) |\
			 (((quad_t)(X) >> 40) & ULL(0x000000000000ff00)) |\
			 (((quad_t)(X) >> 56) & ULL(0x00000000000000ff)))

#if 0
/* data swapping functions, from big/little to little/big endian format */
#if 0 /* FIXME: disabled until further notice... */
#define __SWAP_HALF(N)	((((N) & 0xff) << 8) | (((unsigned short)(N)) >> 8))
#define SWAP_HALF(N)	(sim_swap_bytes ? __SWAP_HALF(N) : (N))

#define __SWAP_WORD(N)	(((N) << 24) |					\
			 (((N) << 8) & 0x00ff0000) |			\
			 (((N) >> 8) & 0x0000ff00) |			\
			 (((unsigned int)(N)) >> 24))
#define SWAP_WORD(N)	(sim_swap_bytes ? __SWAP_WORD(N) : (N))
#else
#define SWAP_HALF(N)	(N)
#define SWAP_WORD(N)	(N)
#endif
#endif
/* recognized endian formats */
enum endian_t { endian_big, endian_little, endian_unknown};
/* probe host (simulator) byte endian format */
enum endian_t
endian_host_byte_order(void);

/* probe host (simulator) double word endian format */
enum endian_t
endian_host_word_order(void);

#ifndef HOST_ONLY

/* probe target (simulated program) byte endian format, only
   valid after program has been loaded */
enum endian_t
endian_target_byte_order(void);

/* probe target (simulated program) double word endian format,
   only valid after program has been loaded */
enum endian_t
endian_target_word_order(void);

#endif /* HOST_ONLY */

#endif /* ENDIAN_H */
