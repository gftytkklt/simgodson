# Makefile - simulator suite make file
#
# This file is part of the godson2 simulator tool suite written by
# godson cpu group of ICT.
# It has been written by extending the SimpleScalar tool suite written by
# Todd M. Austin as a part of the Multiscalar Research Project.
#  
# Copyright (C) 1994, 1995, 1996, 1997, 1998 by Todd M. Austin
#
# Copyright (C) 1999 by Raj Desikan
#
# Copyright (C) 2004 by Fuxin Zhang
#
# This source file is distributed "as is" in the hope that it will be
# useful.  It is distributed with no warranty, and no author or
# distributor accepts any responsibility for the consequences of its
# use. 
#
# Everyone is granted permission to copy, modify and redistribute
# this source file under the following conditions:
#
#    This tool set is distributed for non-commercial use only. 
#    Please contact the maintainer for restrictions applying to 
#    commercial use of these tools.
#
#    Permission is granted to anyone to make or distribute copies
#    of this source code, either as received or modified, in any
#    medium, provided that all copyright notices, permission and
#    nonwarranty notices are preserved, and that the distributor
#    grants the recipient permission for further redistribution as
#    permitted by this document.
#
#    Permission is granted to distribute this file in compiled
#    or executable form under the same conditions that apply for
#    source code, provided that either:
#
#    A. it is accompanied by the corresponding machine-readable
#       source code,
#    B. it is accompanied by a written offer, with no time limit,
#       to give anyone a machine-readable copy of the corresponding
#       source code in return for reimbursement of the cost of
#       distribution.  This written offer must permit verbatim
#       duplication by anyone, or
#    C. it is distributed by someone who received only the
#       executable form, and is accompanied by a copy of the
#       written offer of source code that they received concurrently.
#
# In other words, you are welcome to use, share and improve this
# source file.  You are forbidden to forbid anyone else to use, share
# and improve what you give them.
#
#
# 

##################################################################
#
# Modify the following definitions to suit your build environment,
# NOTE: most platforms should not require any changes
#
##################################################################

#
# Define below C compiler and flags, machine-specific flags and libraries,
# build tools and file extensions, these are specific to a host environment,
# pre-tested environments follow...
#

##
## vanilla Unix, GCC build
##
## NOTE: the Alphasim simulator must be compiled with an ANSI C
## compatible compiler.
##
## tested hosts:
##
##	Slackware Linux version 2.0.33, GNU GCC version 2.7.2.2
##	FreeBSD version 3.0-current, GNU egcs version 2.91.50
##	Alpha OSF1 version 4.0, GNU GCC version 2.7.2
##	PA-RISC HPUX version B.10.01, GNU GCC version 2.7-96q3
##	SPARC SunOS version 5.5.1, GNU egcs-2.90.29
##	RS/6000 AIX Unix version 4, GNU GCC version cygnus-2.7-96q4
##	Windows NT version 4.0, Cygnus CygWin/32 beta 19
##

## ROUTER_FILE: noc_wf or noc_rr
ROUTER_FILE = noc_rr

## noc or noc_worm
ROUTER_FILE1 = noc

CONFLAGS = # -DPOWER_STAT

CC = gcc
#OFLAGS = -O3 -DNDEBUG -Wall
SYSFLAGS = `./sysprobe`
OFLAGS = -g -DDEBUG -DMESI -Wall $(SYSFLAGS) -m32 #-DASYNC_DVFS 
MLIBS = `./sysprobe -libs` -lm
ENDIAN = `./sysprobe -s`
MAKE = make
AR = ar qcv
AROPT =
RANLIB = ranlib
RM = rm -f
RMDIR = rm -f
LN = ln -s
LNDIR = ln -s
DIFF = diff
OEXT = o
LEXT = a
EEXT =
CS = ;
X=/
SFLAGS=

#
# Compilation-specific feature flags
#
# -DDEBUG	- turns on debugging features
# -DBFD_LOADER	- use libbfd.a to load programs (also required BINUTILS_INC
#		  and BINUTILS_LIB to be defined, see below)
# -DGZIP_PATH	- specifies path to GZIP executable, only needed if SYSPROBE
#		  cannot locate binary
# -DSLOW_SHIFTS	- emulate all shift operations, only used for testing as
#		  sysprobe will auto-detect if host can use fast shifts
#
FFLAGS = -DL2BYPASS 
#-DHOST_HAS_QUAD 
#-DFLEXIBLE_SIM

# If enabling this flag, make sure to run the simulator w/o early inst retire
# and use eio traces
#-DFUNC_DEBUG


#
# Point the Makefile to your Simplescalar-based bunutils, these definitions
# should indicate where the include and library directories reside.
# NOTE: these definitions are only required if BFD_LOADER is defined.
#
#BINUTILS_INC = -I../include
#BINUTILS_LIB = -L../lib

#
#


##################################################################
#
# YOU SHOULD NOT NEED TO MODIFY ANYTHING BELOW THIS COMMENT
#
##################################################################

#
# complete flags
#
CFLAGS = $(MFLAGS) $(FFLAGS) $(OFLAGS) $(BINUTILS_INC) $(BINUTILS_LIB) $(CONFLAGS)

#
# all the sources
#
SRCS =	main.c memory.c regs.c  resource.c endian.c symbol.c eval.c \
	options.c range.c stats.c endian.c misc.c mips.c syscall.c fetch.c \
	issue.c writeback.c map.c decode.c commit.c bpred.c loader.c \
	simulate.c tlb.c eventq.c bus.c cache_timing.c eio.c \
	cache.c dram.c lsq.c ptrace.c mem.c cache2mem.c \
	sampling.c istat.c power.c power_model.c $(ROUTER_FILE1).c $(ROUTER_FILE).c extras.c

HDRS =	syscall.h memory.h regs.h sim.h loader.h cache.h \
	resource.h endian.h symbol.h eval.h bitmap.h bus.h cache.h\
	range.h version.h endian.h misc.h mips.h ecoff.h godson2_cpu.h \
	mips.def options.h eio.h bpred.h host.h stats.h mshr.h eventq.h \
	tlb.h dram.h ptrace.h mem.h cache2mem.h sampling.h istat.h power.h \
	noc.h 
#
# common objects
#
OBJS = 	main.$(OEXT) memory.$(OEXT) regs.$(OEXT) resource.$(OEXT) \
	symbol.$(OEXT) eval.$(OEXT) options.$(OEXT) range.$(OEXT) \
	stats.$(OEXT) endian.$(OEXT) misc.$(OEXT) mips.$(OEXT) \
	syscall.$(OEXT) fetch.$(OEXT) issue.$(OEXT) writeback.$(OEXT) \
	map.$(OEXT) decode.$(OEXT) commit.$(OEXT) bpred.$(OEXT) loader.$(OEXT) \
	simulate.$(OEXT) tlb.$(OEXT) eventq.$(OEXT)  cache_timing.$(OEXT) \
	bus.$(OEXT) eio.$(OEXT) cache.$(OEXT) dram.$(OEXT) lsq.$(OEXT) cache2mem.$(OEXT) \
	ptrace.$(OEXT) mem.$(OEXT) sampling.$(OEXT) istat.$(OEXT) power.$(OEXT) power_model.$(OEXT) \
	$(ROUTER_FILE1).$(OEXT) $(ROUTER_FILE).$(OEXT) extras.$(OEXT)

#
# programs to build
#
PROGS = sim-godson$(EEXT) 

#
# all targets, NOTE: library ordering is important...
#
all: $(PROGS)
	@echo "my work is done here..."

sysprobe$(EEXT):	sysprobe.c
	$(CC) $(FFLAGS) -o sysprobe$(EEXT) sysprobe.c
	@echo endian probe results: $(ENDIAN)
	@echo probe flags: $(MFLAGS)
	@echo probe libs: $(MLIBS)

flexible:  SFLAGS=-DFLEXIBLE_SIM 
flexible: sysprobe$(EEXT) $(OBJS) libexo/libexo.$(LEXT)
	condor_compile $(CC) -o sim-godson$(EEXT) $(CFLAGS) $(OBJS) libexo/libexo.$(LEXT) $(MLIBS)

functional: SFLAGS=-DFUNC_DEBUG
functional: sysprobe$(EEXT) $(OBJS) libexo/libexo.$(LEXT)
	$(CC) -o sim-godson$(EEXT) $(CFLAGS) $(OBJS) libexo/libexo.$(LEXT) $(MLIBS)

sim-godson$(EEXT):	sysprobe$(EEXT)  $(OBJS) libexo/libexo.$(LEXT) cacti/libcacti.$(LEXT) 
	$(CC) -o sim-godson$(EEXT) $(CFLAGS) $(OBJS) libexo/libexo.$(LEXT) cacti/libcacti.$(LEXT) $(MLIBS)

exo libexo/libexo.$(LEXT): sysprobe$(EEXT)
	cd ./libexo $(CS) \
	$(MAKE) "MAKE=$(MAKE)" "CC=$(CC)" "AR=$(AR)" "AROPT=$(AROPT)" "RANLIB=$(RANLIB)" "CFLAGS=$(MFLAGS) $(FFLAGS) $(OFLAGS)" "OEXT=$(OEXT)" "LEXT=$(LEXT)" "EEXT=$(EEXT)" "X=$(X)" "RM=$(RM)" libexo.$(LEXT)

 cacti cacti/libcacti.$(LEXT): sysprobe$(EEXT)
	cd cacti $(CS) \
        $(MAKE) "MAKE=$(MAKE)" "CC=$(CC)" "AR=$(AR)" "AROPT=$(AROPT)" "RANLIB=$(RANLIB)" "CFLAGS=$(MFLAGS) $(FFLAGS) $(SAFEOFLAGS)" "OEXT=$(OEXT)" "LEXT=$(LEXT)" "EEXT=$(EEXT)" "X=$(X)" "RM=$(RM)" libcacti.$(LEXT)
	 

.c.$(OEXT):
	$(CC) $(CFLAGS) $(SFLAGS) -c $*.c

filelist:
	@echo $(SRCS) $(HDRS) Makefile

diffs:
	-rcsdiff RCS/*

clean:
	-$(RM) *.o *.obj core *~ Makefile.bak sysprobe$(EEXT) $(PROGS)
	cd ./libexo $(CS) $(MAKE) "RM=$(RM)" "CS=$(CS)" clean $(CS) cd ..
unpure:
	rm -f sim.pure *pure*.o sim.pure.pure_hardlink sim.pure.pure_linkinfo

depend:
	makedepend -n -x $(BINUTILS_INC) $(SRCS)

get:
	rsync -avuzb --exclude '*~' rsync://fxzhang@210.77.27.126/ss-godson/  .
put:
	rsync -Cavuzb . rsync://fxzhang@210.77.27.126/ss-godson/
sync: get put



# DO NOT DELETE THIS LINE -- make depend depends on it.
