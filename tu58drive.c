//
// tu58 - Emulate a TU58 over a serial line
//
// Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
// Update   (C) 2005-2016 Donald N North <ak6dn_at_mindspring_dot_com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// o Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// o Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// o Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// This is the TU58 emulation program written at Rockefeller Univ., Dept. of
// Neurobiology. We copyright (C) it and permit its use provided it is not
// sold to others. Originally written by Dan Ts'o circa 1984 or so.



//
// TU58 Drive Emulator Machine
//



#include "common.h"

#include <pthread.h>

#include "tu58.h"

#ifdef MACOSX
// clock_gettime() is not available under MACOSX
#define CLOCK_REALTIME 1
#include <mach/mach_time.h>
#include <mach/clock.h>
#include <mach/mach.h>

void clock_gettime (int dummy, struct timespec *t) {
    uint64_t mt;
    mt = mach_absolute_time();
    t->tv_sec  = mt / 1000000000;
    t->tv_nsec = mt % 1000000000;
}
#endif


// delays for modeling device access

static struct {
    uint16_t	nop;	// ms per NOP, STATUS commands
    uint16_t	init;	// ms per INIT command
    uint16_t	test;	// ms per DIAGNOSE command
    uint16_t	seek;	// ms per SEEK command (s.b. variable)
    uint16_t	read;	// ms per READ 128B packet command
    uint16_t	write;	// ms per WRITE 128B packet command
} tudelay[] = {
//    nop init test  seek read write
    {  1,   1,   1,    0,   0,    0 }, // timing=0 infinitely fast...
    {  1,   1,  25,   25,  25,   25 }, // timing=1 fast enough to fool diagnostic
    {  1,   1,  25,  200, 100,  100 }, // timing=2 closer to real TU58 behavior
};

// global state

uint8_t mrsp = 0;			// set nonzero to indicate MRSP mode is active

// local state

static uint8_t doinit = 0;	// set nonzero to indicate should send INITs continuously
static uint8_t runonce = 0;	// set nonzero to indicate emulator has been run
static pthread_t th_run;	// emulator thread id
static pthread_t th_monitor;	// monitor thread id



//
// delay routine
//
static void delay_ms (int32_t ms)
{
    timespec_t rqtp;
    int32_t sts;

    // check if any delay required
    if (ms <= 0) return;

    // compute integer seconds and fraction (in nanoseconds)
    rqtp.tv_sec = ms / 1000L;
    rqtp.tv_nsec = (ms % 1000L) * 1000000L;

    // if nanosleep() fails then just plain sleep()
    if ((sts = nanosleep(&rqtp, NULL)) == -1) sleep(rqtp.tv_sec);

    return;
}



//
// reinitialize TU58 state
//
static void reinit (void)
{
    // clear all buffers, wait a bit
    devrxinit();
    devtxinit();
    delay_ms(5);

    // init sequence, send immediately
    devtxstart();
    devtxput(TUF_INIT);
    devtxput(TUF_INIT);
    devtxflush();

    return;
}



//
// read of boot is not packetized, is just raw data
//
static void bootio (void)
{
    int32_t unit;
    int32_t count;
    uint8_t buffer[TU_BOOT_LEN];

    // check unit number for validity
    unit = devrxget();
    if (fileunit(unit)) {
	error("bootio bad unit %d", unit);
	return;
    }

    if (verbose) info("%-8s unit=%d blk=0x%04X cnt=0x%04X", "boot", unit, 0, TU_BOOT_LEN);

    // seek to block zero, should never be an error :-)
    if (fileseek(unit, 0, 0, 0)) {
	error("boot seek error unit %d", unit);
	return;
    }

    // read one block of data
    if ((count = fileread(unit, buffer, TU_BOOT_LEN)) != TU_BOOT_LEN) {
	error("boot file read error unit %d, expected %d, received %d", unit, TU_BOOT_LEN, count);
	return;
    }

    // write one block of data to serial line
    if ((count = devtxwrite(buffer, TU_BOOT_LEN)) != TU_BOOT_LEN) {
	error("boot serial write error unit %d, expected %d, received %d", unit, TU_BOOT_LEN, count);
	return;
    }

    return;
}



//
// debug dump a packet to stderr
//
static void dumppacket (tu_packet *pkt, char *name)
{
    int32_t count = 0;
    uint8_t *ptr = (uint8_t *)pkt;

    // formatted packet dump, but skip it in background mode
    if (!background) {
	fprintf(stderr, "info: %s()\n", name);
	while (count++ < pkt->cmd.length+2) {
	    if (count == 3 || ((count-4)%32 == 31)) fprintf(stderr, "\n");
	    fprintf(stderr, " %02X", *ptr++);
	}
	fprintf(stderr, "\n %02X %02X\n", ptr[0], ptr[1]);
    }

    return;
}



//
// compute checksum of a TU58 packet
//
static uint16_t checksum (tu_packet *pkt)
{
    int32_t count = pkt->cmd.length + 2; // +2 for flag/length bytes
    uint8_t *ptr = (uint8_t *)pkt; // start at flag byte
    uint32_t chksum = 0; // initial checksum value

    while (--count >= 0) {
	chksum += *ptr++; // at least one byte
	if (--count >= 0) chksum += (*ptr++ << 8); // at least two bytes
	chksum = (chksum + (chksum >> 16)) & 0xFFFF; // 16b end around carry
    }

    return chksum;
}



//
// wait for a CONT to arrive
//
static void wait4cont (uint8_t code)
{
    uint8_t c;
    int32_t maxchar = TU_CTRL_LEN+TU_DATA_LEN+8;

    // send any existing data out ... makes USB serial emulation be real slow if enabled!
    if (0) devtxflush();

    // don't do any waiting if flag not set
    if (!code) return;

    // wait for a CONT to arrive, but only so long
    do {
	c = devrxget();
	if (debug) info("wait4cont(): char=0x%02X", c);
    } while (c != TUF_CONT && --maxchar >= 0);

    // all done
    return;
}



//
// put a packet
//
static void putpacket (tu_packet *pkt)
{
    int32_t count = pkt->cmd.length + 2; // +2 for flag/length bytes
    uint8_t *ptr = (uint8_t *)pkt; // start at flag byte
    uint16_t chksum;

    // send all packet bytes
    while (--count >= 0) {
	devtxput(*ptr++);
	wait4cont(mrsp);
    }

    // compute/send checksum bytes, append to packet
    chksum = checksum(pkt);
    devtxput(*ptr++ = chksum>>0);
    wait4cont(mrsp);
    devtxput(*ptr++ = chksum>>8);
    wait4cont(mrsp);
    
    // for debug...
    if (debug) dumppacket(pkt, "putpacket");

    // now actually send the packet (or whatever is left to send)
    devtxflush();

    return;
}



//
// get a packet
//
static int32_t getpacket (tu_packet *pkt)
{
    int32_t count = pkt->cmd.length + 2; // +2 for checksum bytes
    uint8_t *ptr = (uint8_t *)pkt + 2; // skip over flag/length bytes
    uint16_t rcvchk, expchk;

    // get remaining packet bytes, incl two checksum bytes
    while (--count >= 0) *ptr++ = devrxget();

    // get checksum bytes
    rcvchk = (ptr[-1]<<8) | (ptr[-2]<<0);

    // compute expected checksum
    expchk = checksum(pkt);

    // for debug...
    if (debug) dumppacket(pkt, "getpacket");

    // message on error
    if (expchk != rcvchk)
	error("getpacket checksum error: exp=0x%04X rcv=0x%04X", expchk, rcvchk);

    // return checksum match indication
    return (expchk != rcvchk);
}



//
// tu58 sends end packet to host
//
static void endpacket (uint8_t unit,
		       uint8_t code,
		       uint16_t count,
		       uint16_t status)
{
    static tu_cmdpkt ek = { TUF_CTRL, TU_CTRL_LEN, TUO_END, 0, 0, 0, 0, 0, 0, -1 };

    ek.unit = unit;
    ek.modifier = code; // success/fail code
    ek.count = count;
    ek.block = status; // summary status

    putpacket((tu_packet *)&ek);
    devtxflush(); // finish packet transmit

    return;
}



//
// return requested block size of a tu58 access
//
static inline int32_t blocksize (uint8_t modifier)
{
    return (modifier & TUM_B128) ? BLOCKSIZE/4 : BLOCKSIZE;
}



//
// host seek of tu58
//
static void tuseek (tu_cmdpkt *pk)
{
    // check unit number for validity
    if (fileunit(pk->unit)) {
	error("tuseek bad unit %d", pk->unit);
	endpacket(pk->unit, TUE_BADU, 0, 0);
	return;
    }

    // seek to desired block
    if (fileseek(pk->unit, blocksize(pk->modifier), pk->block, 0)) {
	error("tuseek unit %d bad block 0x%04X", pk->unit, pk->block);
	endpacket(pk->unit, TUE_BADB, 0, 0);
	return;
    }

    // fake a seek time
    delay_ms(tudelay[timing].seek);

    // success if we get here
    endpacket(pk->unit, TUE_SUCC, 0, 0);

    return;
}



//
// host read from tu58
//
static void turead (tu_cmdpkt *pk)
{
    int32_t count;
    tu_datpkt dk;

    // check unit number for validity
    if (fileunit(pk->unit)) {
	error("turead bad unit %d", pk->unit);
	endpacket(pk->unit, TUE_BADU, 0, 0);
	return;
    }

    // seek to desired ending block offset
    if (fileseek(pk->unit, blocksize(pk->modifier), pk->block, pk->count-1)) {
	error("turead unit %d bad block 0x%04X", pk->unit, pk->block);
	endpacket(pk->unit, TUE_BADB, 0, 0);
	return;
    }

    // seek to desired starting block offset
    if (fileseek(pk->unit, blocksize(pk->modifier), pk->block, 0)) {
	error("turead unit %d bad block 0x%04X", pk->unit, pk->block);
	endpacket(pk->unit, TUE_BADB, 0, 0);
	return;
    }

    // fake a seek time
    delay_ms(tudelay[timing].seek);

    // send data in packets until we run out
    for (count = pk->count; count > 0; count -= dk.length) {

	// max bytes to send at once is TU_DATA_LEN
	dk.flag = TUF_DATA;
	dk.length = count < TU_DATA_LEN ? count : TU_DATA_LEN;

	if (fileread(pk->unit, dk.data, dk.length) == dk.length) {
	    // successful file read, send packet
	    putpacket((tu_packet *)&dk);
	    // fake a read time
	    delay_ms(tudelay[timing].read);
	} else {
	    // whoops, something bad happened
	    error("turead unit %d data error block 0x%04X count 0x%04X",
		  pk->unit, pk->block, pk->count);
	    endpacket(pk->unit, TUE_PARO, pk->count-count, 0);
	    return;
	}
    }

    // success if we get here
    endpacket(pk->unit, TUE_SUCC, pk->count, 0);

    return;
}


 
//
// host write to tu58
//
static void tuwrite (tu_cmdpkt *pk)
{
    int32_t count;
    int32_t status;
    tu_datpkt dk;

    // check unit number for validity
    if (fileunit(pk->unit)) {
	error("tuwrite bad unit %d", pk->unit);
	endpacket(pk->unit, TUE_BADU, 0, 0);
	return;
    }

    // seek to desired ending block offset
    if (fileseek(pk->unit, blocksize(pk->modifier), pk->block, pk->count-1)) {
	error("tuwrite unit %d bad block 0x%04X", pk->unit, pk->block);
	endpacket(pk->unit, TUE_BADB, 0, 0);
	return;
    }

    // seek to desired starting block offset
    if (fileseek(pk->unit, blocksize(pk->modifier), pk->block, 0)) {
	error("tuwrite unit %d bad block 0x%04X", pk->unit, pk->block);
	endpacket(pk->unit, TUE_BADB, 0, 0);
	return;
    }

    // fake a seek time
    delay_ms(tudelay[timing].seek);

    // keep looping if more data is expected
    for (count = pk->count; count > 0; count -= dk.length) {

	// send continue flag; we are ready for more data
	devtxput(TUF_CONT);
	devtxflush();
	if (debug) info("sending <CONT>");

	uint8_t last;
	dk.flag = -1;

	// loop until we see data flag
	do {
	    last = dk.flag;
	    dk.flag = devrxget();
	    if (debug) info("flag=0x%02X last=0x%02X", dk.flag, last);
	    if (last == TUF_INIT && dk.flag == TUF_INIT) {
		// two in a row is special
		devtxput(TUF_CONT); // send 'continue'
		devtxflush(); // send immediate
		if (debug) info("<INIT><INIT> seen, sending <CONT>, abort write");
		return; // abort command
	    } else if (dk.flag == TUF_CTRL) {
		error("protocol error, unexpected CTRL flag during write");
		endpacket(pk->unit, TUE_DERR, 0, 0);
		return;
	    } else if (dk.flag == TUF_XOFF) {
		if (debug) info("<XOFF> seen, stopping output");
		devtxstop();
	    } else if (dk.flag == TUF_CONT) {
		if (debug) info("<CONT> seen, starting output");
		devtxstart();
	    }
	} while (dk.flag != TUF_DATA);

	// byte following data flag is packet data length
	dk.length = devrxget();

	// get remainder of the data packet
	if (getpacket((tu_packet *)&dk)) {
	    // whoops, checksum error, fail
	    error("data checksum error");
	    endpacket(pk->unit, TUE_DERR, 0, 0);
	    return;
	}

	// write data packet to file
	if ((status = filewrite(pk->unit, dk.data, dk.length)) != dk.length) {
	    if (status == -2) {
		// whoops, unit is write protected
		error("tuwrite unit %d is write protected block 0x%04X count 0x%04X",
		      pk->unit, pk->block, pk->count);
		endpacket(pk->unit, TUE_WPRO, pk->count-count, 0);
	    } else {
		// whoops, some other data write error (like past EOF)
		error("tuwrite unit %d data write error block 0x%04X count 0x%04X",
		      pk->unit, pk->block, pk->count);
		endpacket(pk->unit, TUE_PARO, pk->count-count, 0);
	    }
	    return;
	}

	// fake a write time
	delay_ms(tudelay[timing].write);
    }

    // must fill out last block with zeros
    if ((count = pk->count % blocksize(pk->modifier)) > 0) {
	uint8_t buffer[BLOCKSIZE];
	bzero(buffer, (count = blocksize(pk->modifier)-count));
	if (debug) info("tuwrite unit %d filling %d zeroes", pk->unit, count);
	if (filewrite(pk->unit, buffer, count) != count) {
	    // whoops, something bad happened
	    error("tuwrite unit %d data error block 0x%04X count 0x%04X",
		  pk->unit, pk->block, pk->count);
	    endpacket(pk->unit, TUE_PARO, pk->count, 0);
	    return;
	}
	// fake a write time
	delay_ms(tudelay[timing].write);
    }

    // success if we get here
    endpacket(pk->unit, TUE_SUCC, pk->count, 0);

    return;
}



//
// decode and execute control packets
//
static void command (int8_t flag)
{
    tu_cmdpkt pk;
    timespec_t time_start;
    timespec_t time_end;
    char *name= "none";
    uint8_t mode = 0;

    // avoid uninitialized variable warnings
    time_start.tv_sec  = 0;
    time_start.tv_nsec = 0;
    time_end.tv_sec    = 0;
    time_end.tv_nsec   = 0;

    pk.flag = flag;
    pk.length = devrxget();

    // check control packet length ... if too long flush it
    if (pk.length > sizeof(tu_cmdpkt)) {
	error("bad length 0x%02X in cmd packet", pk.length);
	reinit();
	return;
    }

    // check packet checksum ... if bad error it
    if (getpacket((tu_packet *)&pk)) {
	error("cmd checksum error");
	endpacket(pk.unit, TUE_DERR, 0, 0);
	return;
    }

    if (debug) info("opcode=0x%02X length=0x%02X", pk.opcode, pk.length);

    // dump command if requested
    if (verbose) {

	// parse commands to classes
	switch (pk.opcode) {
	case TUO_DIAGNOSE:  name = "diagnose"; mode = 1; break;
	case TUO_GETCHAR:   name = "getchar";  mode = 1; break;
	case TUO_INIT:      name = "init";     mode = 1; break;
	case TUO_NOP:       name = "nop";      mode = 1; break;
	case TUO_GETSTATUS: name = "getstat";  mode = 1; break;
	case TUO_SETSTATUS: name = "setstat";  mode = 1; break;
	case TUO_SEEK:      name = "seek";     mode = 2; break;
	case TUO_READ:      name = "read";     mode = 3; break;
	case TUO_WRITE:     name = "write";    mode = 3; break;
	default:            name = "unknown";  mode = 3; break;
	}

	// dump data
	switch (mode) {
	case 0:
	    info("%-8s", name);
	    break;
	case 1:
	    info("%-8s unit=%d", name, pk.unit);
	    break;
	case 2:
	    info("%-8s unit=%d sw=0x%02X mod=0x%02X blk=0x%04X",
		 name, pk.unit, pk.switches, pk.modifier, pk.block);
	    break;
	case 3:
	    info("%-8s unit=%d sw=0x%02X mod=0x%02X blk=0x%04X cnt=0x%04X",	
		 name, pk.unit, pk.switches, pk.modifier, pk.block, pk.count);
	    break;
	}

	// get start time of processing
	clock_gettime(CLOCK_REALTIME, &time_start);

    }

    // if we are MRSP capable, look at the switches
    if (mrspen) mrsp = (pk.switches & TUS_MRSP) ? 1 : 0;

    // decode packet
    switch (pk.opcode) {

    case TUO_READ: // read data from tu58
	turead(&pk);
	break;

    case TUO_WRITE: // write data to tu58
	tuwrite(&pk);
	break;

    case TUO_SEEK: // reposition tu58
	tuseek(&pk);
	break;

    case TUO_DIAGNOSE: // diagnose packet
	delay_ms(tudelay[timing].test);
	endpacket(pk.unit, TUE_SUCC, 0, 0);
	break;

    case TUO_GETCHAR: // get characteristics packet
	delay_ms(tudelay[timing].nop);
	if (mrspen) {
	    // MRSP capable just sends the end packet
	    endpacket(pk.unit, TUE_SUCC, 0, 0);
	} else {
	    // MRSP detect mode not enabled
	    // indicate we are not MRSP capable
	    tu_datpkt dk;
	    dk.flag = TUF_DATA;
	    dk.length = TU_CHAR_LEN;
	    bzero(dk.data, dk.length);
	    putpacket((tu_packet *)&dk);
	}
	break;

    case TUO_INIT: // init packet
	delay_ms(tudelay[timing].init);
	devtxinit();
	devrxinit();
	endpacket(pk.unit, TUE_SUCC, 0, 0);
	break;

    case TUO_NOP: // nop packet
    case TUO_GETSTATUS: // get status packet
    case TUO_SETSTATUS: // set status packet
	delay_ms(tudelay[timing].nop);
	endpacket(pk.unit, TUE_SUCC, 0, 0);
	break;

    default: // unknown packet
	delay_ms(tudelay[timing].nop);
	endpacket(pk.unit, TUE_BADO, 0, 0);
	break;

    }

    if (verbose) {

	uint32_t delta;

	// get end time of processing
	clock_gettime(CLOCK_REALTIME, &time_end);

	// compute elapsed time in milliseconds
	delta = 1000L*(time_end.tv_sec - time_start.tv_sec) + (time_end.tv_nsec - time_start.tv_nsec)/1000000L;
	if (delta == 0) delta = 1;

	// print elapsed time in milliseconds
	if (debug) info("%-8s time=%dms", name, delta);

    }

    return;
}



//
// field requests from host
//
static void* run (void* none)
{
    uint8_t flag = TUF_NULL;
    uint8_t last = TUF_NULL;

    // some init
    reinit(); // empty serial line buffers
    doinit = !nosync; // start sending init flags?

    // say hello
    info("emulator %sstarted", runonce++ ? "re" : "");

    // loop forever ... almost
    for (;;) {

	// loop while no characters are available
	while (devrxavail() == 0) {
	    // delays and printout only if not VAX
	    if (!vax) {
		// send INITs if still required
		if (doinit) {
		    if (debug) fprintf(stderr, ".");
		    devtxput(TUF_INIT);
		    devtxflush();
		    delay_ms(75);
		}
		delay_ms(25);
	    }
	}
	doinit = 0; // quit sending init flags

	// process received characters
	last = flag;
	flag = devrxget();
	if (debug) info("flag=0x%02X last=0x%02X", flag, last);

	switch (flag) {

	case TUF_CTRL:
	    // control packet - process
	    command(flag);
	    break;

	case TUF_INIT:
	    // init flag
	    if (debug) info("<INIT> seen");
	    if (last == TUF_INIT) {
		// two in a row is special
		if (!vax) delay_ms(tudelay[timing].init); // no delay for VAX
		devtxput(TUF_CONT); // send 'continue'
		devtxflush(); // send immediate
		flag = -1; // undefined
		if (debug) info("<INIT><INIT> seen, sending <CONT>");
	    }
	    break;

	case TUF_BOOT:
	    // special boot sequence
	    if (debug) info("<BOOT> seen");
	    bootio();
	    break;

	case TUF_NULL:
	    // ignore nulls (which are BREAKs)
	    if (debug) info("<NULL> seen");
	    break;

	case TUF_CONT:
	    // continue restarts output
	    if (debug) info("<CONT> seen, starting output");
	    devtxstart();
	    break;

	case TUF_XOFF:
	    // send disable flag stops output
	    if (debug) info("<XOFF> seen, stopping output");
	    devtxstop();
	    break;

	case TUF_DATA:
	    // data packet - should never see one here
	    error("protocol error - data flag out of sequence");
	    reinit();
	    break;

	default:
	    // whoops, protocol error
	    error("unknown packet flag 0x%02X (%c)", flag, isprint(flag)?flag:'.');
	    break;

	} // switch (flag)

    } // for (;;)

    return (void*)0;
}



//
// monitor for break/error on line, restart emulator if seen
//
static void* monitor (void* none)
{
    int32_t sts;

    for (;;) {

	// check for any error
	switch (sts = devrxerror()) {
	case DEV_ERROR: // error
	case DEV_BREAK: // break
	    // kill and restart the emulator
	    if (verbose) info("BREAK detected");
#ifdef THIS_DOES_NOT_YET_WORK_RELIABLY
	    if (pthread_cancel(th_run))
		error("unable to cancel emulation thread");
	    if (pthread_join(th_run, NULL))
		error("unable to join on emulation thread");
	    if (pthread_create(&th_run, NULL, run, NULL))
		error("unable to restart emulation thread");
#endif // THIS_DOES_NOT_YET_WORK_RELIABLY
	    break;
	case DEV_OK: // OK
	    break;
	case DEV_NYI: // not yet implemented
	    return (void*)1;
	default: // something else...
	    error("monitor(): unknown flag %d", sts);
	    break;
	}
	// bit of a delay, loop again
	delay_ms(5);

    }

    return (void*)0;
}



//
// start tu58 drive emulation
//
void tu58drive (void)
{
    // a sanity check for blocksize definition
    if (BLOCKSIZE % TU_DATA_LEN != 0)
	fatal("illegal BLOCKSIZE (%d) / TU_DATA_LEN (%d) ratio", BLOCKSIZE, TU_DATA_LEN);

    // say hello
    info("TU58 emulation start");
    info("R restart, S toggle send init, V toggle verbose, D toggle debug, Q quit");

    // run the emulator
    if (pthread_create(&th_run, NULL, run, NULL))
	error("unable to create emulation thread");

    // run the monitor
    if (pthread_create(&th_monitor, NULL, monitor, NULL))
	error("unable to create monitor thread");

    // loop on user input
    for (;;) {
	uint8_t c;

	// get char from stdin (if available)
	if ((c = toupper(conget())) > 0) {
	    if (c == 'V') {
		// toggle verbosity
		verbose ^= 1;  debug = 0;
		info("verbosity set to %s; debug %s",
		     verbose ? "ON" : "OFF", debug ? "ON" : "OFF");
	    } else if (c == 'D') {
		// toggle debug
		verbose = 1;  debug ^= 1;
		info("verbosity set to %s; debug %s",
		     verbose ? "ON" : "OFF", debug ? "ON" : "OFF");
	    } else if (c == 'S') {
		// toggle sending init string
		doinit = (doinit+1)%2;
		if (debug) fprintf(stderr, "\n");
		info("send of <INIT> %sabled", doinit ? "en" : "dis");
	    } else if (c == 'R') {
		// kill and restart the emulator
		if (pthread_cancel(th_run))
		    error("unable to cancel emulation thread");
		if (pthread_join(th_run, NULL))
		    error("unable to join on emulation thread");
		if (pthread_create(&th_run, NULL, run, NULL))
		    error("unable to restart emulation thread");
	    } else if (c == 'Q') {
		// kill the emulator and exit
		if (pthread_cancel(th_monitor))
		    error("unable to cancel monitor thread");
		if (pthread_cancel(th_run))
		    error("unable to cancel emulation thread");
		break;
	    }
	}

	// wait a bit
	delay_ms(25);

    } // for (;;)

    // wait for emulator to finish
    if (pthread_join(th_run, NULL))
	error("unable to join on emulation thread");

    // all done
    info("TU58 emulation end");
    return;
}



// the end
