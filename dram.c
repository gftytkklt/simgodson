/*
 * dram.c - dram access routines. simulates sdram/ddr
 *
 * this file is being contributed to the SimpleScalar tool suite written by
 * Todd M. Austin as a part of the Multiscalar Research Project.
 *
 * Copyright (c) 2001, 2000, 1999 by Vinodh Cuppu and Bruce Jacob
 * Vinodh Cuppu, Bruce Jacob
 * Systems and Computer Architecture Lab
 * Dept of Electrical & Computer Engineering
 * University of Maryland, College Park
 * All Rights Reserved
 *
 * Distributed as part of the sim-alpha disribution
 * Copyright (C) 1999 by Raj Desikan
 * This software is distributed with *ABSOLUTELY NO SUPPORT* and
 * *NO WARRANTY*.  Permission is given to modify this code
 * as long as this notice is not removed.
 *
 * Send feedback to Vinodh Cuppu ramvinod@eng.umd.edu / ramvinod@computer.org
 *               or Bruce Jacob blj@eng.umd.edu
 */

#include <stdio.h>
#include "mips.h"
#include "dram.h"

/*
 * some dram parameters that can be set
 */

/*
 * cpu freq / dram freq
 * this is used to convert sdram cycles to cpu cycles
 */
int clock_multiplier;

/*
 * inside a dram bank, do we keep the row open or close it after each access
 * 0 - open page
 * 1 - close page auto precharge 
 */
int page_policy;

/*
 * time between start of ras command and start of cas command
 * in sdram cycles
 */
int ras_delay;

/*
 * time between start of cas command and start of first chunk of data
 * in sdram cycles
 */
int cas_delay;

/*
 * time between start of precharge command and start of ras command
 */
int pre_delay;

/*
 * is data double-data-rate ?
 * 0 - no
 * 1 - yes
 */
int data_rate;

/*
 * cpu cycles delay in chipset once the cpu generates request
 * after L2 lookup resulted in a miss, to start of first dram command
 */
int chipset_delay_req;

/*
 * on the return path, from dram to cpu, is there any delay in chipset
 * where the data is buffered
 */
int chipset_delay_return;

/*
 * how fat is the data bus between cpu - chipset - dram
 * dram width will equal this too.
 */
int bus_width;

/*
 * bank cycle time
 * dont touch this unless you know what you are doing.
 * this the time between 2 ras commands to the same bank.
 */
int rc_delay;

/*
 * here are some more memory and dram parameters
 * let's have 128Mbit SDRAM, 4 chips in parallel in a dimm. so that gives 4x16, 64 bits
 * let's have 4 banks X 4096 rows X 512 columns X 16 DQs per chip
 * let there be 4 dimms in the machine. 
 * so the total memory in the system would be
 * 16 MB (128 Mbit) * 4 * 4 = 256 MB
 * note that this the configuration for a 8 byte wide bus
 */

/* 
 * mips.configuration - 64Mbit SDRAM, 16 chips in parallel in a dimm
 * 2 dimms in the machine 
 * Let's have 4 banks X 4096 rows X 512 columns X 8 DQs per chip
 * 8 MB (64 Mbit) * 16 * 2 = 256 MB
 * This is for a 16 byte wide bus
 */

int row_shift, row_mask;
int bank_shift, bank_mask;
int dimm_shift, dimm_mask;

md_addr_t rowbuffer[10][10];

unsigned int
log2( int n )
{
  int i = 0;
  while( n > 1 )
    {
      i++;
      n = n / 2;
    }
  return( i );
}


void
dram_init()
{
  int num_chips_per_dimm;

  num_chips_per_dimm = bus_width * 8 / 8 /* DQs*/;

  row_shift = log2( 512 ) /*columns*/ + log2( bus_width );
  row_mask  = 4096 /*rows*/ - 1;
  bank_shift = row_shift + log2( 4096) /*rows*/;
  bank_mask = 4 /*banks*/ - 1;
  dimm_shift = bank_shift + log2( 4 ) /*banks*/;
  dimm_mask = 2 /*dimms*/ - 1;

  //fprintf(stderr, "%d %x %d %x %d %x\n", dimm_shift, dimm_mask, bank_shift, bank_mask, row_shift, row_mask);

  memset( rowbuffer, 0, sizeof(rowbuffer) );

}

int
dram_access_latency( int cmd,        /* read or write */
		     md_addr_t addr, /* address to read from */
		     int bsiz,       /* size to read/write */
		     tick_t now )    /* what's the cycle number now */
{

  int dram_latency;
  int latency;
  md_addr_t row, bank, dimm;
  int data_transfers;

  row  = (addr >> row_shift) & row_mask;
  bank = (addr >> bank_shift) & bank_mask;
  dimm = (addr >> dimm_shift) & dimm_mask;

  dram_latency = 0;
  dram_latency += chipset_delay_req;

  if ( page_policy == OP )
    {
      if ( rowbuffer[dimm][bank] == row )
	{
	  /* do nothing */
	}
      else 
	{
	  dram_latency += pre_delay + ras_delay;
	  rowbuffer[dimm][bank] = row;
	}
    }
  else if ( page_policy == CPA )
    {
      dram_latency += ras_delay;
    }
  
  dram_latency += cas_delay;

  data_transfers = bsiz / bus_width;
  if ( data_transfers < 1 )
    data_transfers = 1;

  dram_latency += (data_transfers / data_rate);

  dram_latency += chipset_delay_return;

  latency = dram_latency * clock_multiplier;

  fprintf(stderr, "%x %x %x %x %d %d %lld %d %d\n", dimm, bank, row, addr, cmd, bsiz, now, latency, data_transfers);

  return( latency );
}



