/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */

/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/* ncar_a2d_priv.h

NCAR A/D driver private header

*/

#ifndef NCAR_A2D_PRIV_H
#define NCAR_A2D_PRIV_H

#include <nidas/linux/ncar_a2d.h>       // shared stuff
#include <nidas/linux/filters/short_filters_kernel.h>

#include <linux/wait.h>
#include <linux/device.h>

#define A2D_MAX_RATE    5000

//AD7725 Status register bits
#define A2DINSTBSY      0x8000  //Instruction being performed
#define A2DDATARDY      0x4000  //Data ready to be read (Read cycle)
#define A2DDATAREQ      0x2000  //New data required (Write cycle)
#define A2DIDERR        0x1000  //ID error
#define A2DCRCERR       0x0800  //Data corrupted--CRC error
#define A2DDATAERR      0x0400  //Conversion data invalid
#define A2DINSTREG15    0x0200  //Instr reg bit 15
#define A2DINSTREG13    0x0100  //                              13
#define A2DINSTREG12    0x0080  //                              12
#define A2DINSTREG11    0x0040  //                              11
#define A2DINSTREG06    0x0020  //                              06
#define A2DINSTREG05    0x0010  //                              05
#define A2DINSTREG04    0x0008  //                              04
#define A2DINSTREG01    0x0004  //                              01
#define A2DINSTREG00    0x0002  //                              00
#define A2DCONFIGEND    0x0001  //Configuration End Flag.

#define A2DSTAT_INSTR_MASK      0x3fe   // mask of instruction bits in status

/* values in the AD7725 Status Register should look like so when
 * the A2Ds are running.  The instruction is RdCONV=0x8d21.
 *
 * bit name             value (X=varies)
 * 15 InstrBUSY         1
 * 14 DataReady         X  it's a mystery why this isn't always set
 * 13 DataRequest       0
 * 12 ID Error          0
 * 11 CRC Error         0
 * 10 Data Error        X  indicates input voltage out of range
 *  9 Instr bit 15      1
 *  8 Instr bit 13      0
 *  7 Instr bit 12      0
 *  6 Instr bit 11      1
 *  5 Instr bit 06      0
 *  4 Instr bit 05      1
 *  3 Instr bit 04      0
 *  2 Instr bit 01      0
 *  1 Instr bit 00      1
 *  0 CFGEND            X set when configuration was downloaded
 */

#define MAX_A2D_BOARDS          4       // maximum number of A2D boards

#define HWFIFODEPTH             1024    // # of words in card's hardware FIFO

/**
 * Define this if you want a fixed polling rate of the A2D FIFO.
 * If FIXED_POLL_RATE is not defined, a minimum rate will be chosen
 * so that the maximum is read on each poll, but less than 1/4 the FIFO size.
 *
 * gmaclean, 8 Feb 2012:
 * 50 Hz polling (reading 80 words each time) *may* result in less
 * chance of a buffer overflow on Vulcans than the computed polling
 * rate of 20 Hz (200 words).  Needs testing...
 */
// #define FIXED_POLL_RATE 50

/**
 * Set POLL_WHEN_QUARTER_FULL if you want polling to be delayed
 * by one or more periods so that the FIFO is at least 1/4 full
 * before a poll. Then a poll is guaranteed not to empty the FIFO.
 * If overflows of the FIFO are more of an issue, don't define this,
 * and the polling will be delayed by just one period.
 */
#define POLL_WHEN_QUARTER_FULL

#define A2DMASTER	0       // A/D chip designated to produce interrupts
#define A2DIOWIDTH	0x10    // Width of I/O space

/*
 * address offset for commands
 */
#  define A2DCMDADDR	0xF


// I/O commands for the A/D card
#define A2DIO_FIFO         0x0  // Next IO instr a FIFO data inw or FIFO control outb
#define A2DIO_CS_CMD       0x1  // command read/write of A2D chip
#define A2DIO_I2C          0x2  // i2c read/write
#define A2DIO_DATA         0x3  // data write, gain codes to base+0, fcoeffs to base+chan*2
#define A2DIO_D2A1         0x4  // sent after gains codes have been sent (needed?)
#define A2DIO_CALV         0x5  // cal voltage write
#define A2DIO_SYSCTL       0x6  // read A/D INT lines, write cal/offset bits for 8 channels
#define A2DIO_FIFOSTAT     0x7  // read board status,  set master A/D

// Channel select, add to CS_CMD for write or read of A2D chip
#define A2DIO_CS_WR        0x0  // write, default for CS_CMD
#define A2DIO_CS_RD        0x8  // read, next IO an inw from base+chan*2

#define A2DIO_CS_CMD_RD   (A2DIO_CS_CMD + A2DIO_CS_RD)  // chan select, next an inw from base+chan*2
#define A2DIO_CS_CMD_WR   (A2DIO_CS_CMD + A2DIO_CS_WR)  // chan select, next an outw to base+chan*2

// AD7725 chip command words (See A2DIO_CS_CMD above)
#define AD7725_READID      0x8802       // Read device ID (NOT USED)
#define AD7725_READDATA    0x8d21       // Read converted data
#define AD7725_WRCONFIG    0x1800       // Write configuration data
#define AD7725_WRCONFEM    0x1A00       // Write configuration, mask data (NOT USED)
#define AD7725_ABORT       0x0000       // Soft reset; still configured
#define AD7725_BFIR        0x2000       // Boot from internal ROM (NOT USED)

// A/D Control bits
#define FIFOCLR        0x01     // [FIFOCTL(0)>  Cycle this bit 0-1-0 to clear FIFO
#define A2DAUTO        0x02     // [FIFOCTL(1)>  Set = allow A/D's to run automatically
#define A2DSYNC        0x04     // [FIFOCTL(2)>  Set then cycle A2DSYNCCK to stop A/D's
#define A2DSYNCCK      0x08     // [FIFOCTL(3)>  Cycle to latch A2DSYNC bit value
#define A2D1PPSEBL     0x10     // [FIFOCTL(4)>  Set to allow GPS 1PPS to clear SYNC
#define FIFODAFAE      0x20     // [FIFOCTL(5)>  Set to clamp value of AFAE in FIFO     // NOT USED
#define A2DSTATEBL     0x40     // [FIFOCTL(6)>  Enable A2D status
#define FIFOWREBL      0x80     // [FIFOCTL(7)>  Enable writing to FIFO. (not used)     // NOT USED

// FIFO Status bits
#define FIFOHF         0x01     // FIFO half full
#define FIFOAFAE       0x02     // FIFO almost full/almost empty
#define FIFONOTEMPTY   0x04     // FIFO not empty
#define FIFONOTFULL    0x08     // FIFO not full
#define INV1PPS        0x10     // Inverted 1 PPS pulse
#define PRESYNC        0x20     // Presync bit                   // NOT USED

#define USE_RESET_WORKER

struct a2d_sample
{
        dsm_sample_time_t timetag;      // timetag of sample
        dsm_sample_length_t length;     // number of bytes in data
	unsigned short id;
        short data[NUM_NCAR_A2D_CHANNELS];
};

struct A2DBoard
{
        unsigned int ioport; // Base address of board

        unsigned long base_addr; // Base address of board

        unsigned long base_addr16; // address for 16 bit transfers

        unsigned long cmd_addr;  // Address for commands to the board

        char deviceName[32];

        struct device* device;

        unsigned short    ser_num;        // A/D card serial number

        int gain[NUM_NCAR_A2D_CHANNELS];    // Gain settings
        int offset[NUM_NCAR_A2D_CHANNELS];  // Offset flags
        unsigned short ocfilter[CONFBLOCKS*CONFBLLEN+1]; // on-chip filter data

        int scanRate;           // how fast to scan the channels
        int scanDeltatMsec;     // dT between A2D scans
        int pollDeltatMsec;     // dT between times of polling the FIFO
        int pollRate;           // how fast to poll the FIFO
        int irigRate;           // poll irigClockRate (e.g.: IRIG_100_HZ)
        int nFifoValues;        // How many FIFO values to read every poll
        int skipFactor;         // set to 2 to skip over interleaving status
        int busy;
        int interrupted;
        unsigned int readCtr;
        int master;
        int discardNextScan;	// first A2D values after startup are bad, discard them

	/**
	 * To be sure reads are not done from an empty FIFO,
	 * delay the reads until it is a least 1/4 full.
	 */
	int delaysBeforeFirstPoll;

        int numPollDelaysLeft;

        int totalOutputRate;    // total requested output sample rate

        struct dsm_sample_circ_buf fifo_samples;        // samples for bottom half
        struct dsm_sample_circ_buf a2d_samples; // samples out of b.h.
        wait_queue_head_t rwaitq_a2d;   // wait queue for user reads of a2d
        struct sample_read_state a2d_read_state;

        struct irig_callback* ppsCallback;
	wait_queue_head_t ppsWaitQ;
	int havePPS;

        int nfilters;           // how many different output filters
        struct short_filter_info *filters;

        struct irig_callback* a2dCallback;

        struct irig_callback* tempCallback;

        int tempRate;           // rate to query I2C temperature sensor
        short currentTemp;

        unsigned int nbadFifoLevel;
        unsigned int fifoNotEmpty;
        unsigned int skippedSamples;  // how many samples have we missed?

#ifdef USE_RESET_WORKER
        struct work_struct resetWorker;
#endif
        int errorState;

        int resets;             // number of board resets since driver load

        unsigned short OffCal;  // offset and cal bits
        unsigned char FIFOCtl;  // hardware FIFO control word storage
        unsigned char i2c;      // data byte written to I2C

        struct work_struct sampleWorker;

        long latencyMsecs;      // buffer latency in milli-seconds
        long latencyJiffies;    // buffer latency in jiffies
        unsigned long lastWakeup;       // when were read & poll methods last woken

        struct ncar_a2d_cal_config cal;            // calibration configuration
        struct ncar_a2d_status cur_status;  // status info maintained by driver
        struct ncar_a2d_status prev_status; // status info maintained by driver

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex mutex;         // when setting up irq handler
#else
        struct semaphore mutex;     // when setting up irq handler
#endif
};
#endif
