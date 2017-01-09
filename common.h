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
// TU58 Emulator Common Definitions
//



// Common includes

#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#ifndef O_BINARY
#define O_BINARY 0		// for linux compatibility
#endif

#ifndef timespec_t
typedef struct timespec timespec_t;
#endif



// Constants

#define NTU58		8	// number of devices to emulate (0..N-1)

#define TAPESIZE	512	// number of blocks per tape
#define BLOCKSIZE	512	// number of bytes per block

#define FILEREAD	1	// file can be read
#define FILEWRITE	2	// file can be written
#define FILECREATE	3	// file should be created
#define FILERT11INIT	4	// file should be init'ed as RT11 structure
#define FILEXXDPINIT	5	// file should be init'ed as XXDP structure

#define DEV_NYI		-1	// not yet implemented
#define DEV_OK		 0	// no error
#define DEV_BREAK	 1	// BREAK on line
#define DEV_ERROR	 2	// ERROR on line



// Prototypes

// main.c
void fatal (char *, ...);
void error (char *, ...);
void info (char *, ...);

// serial.c
void devtxbreak (void);
void devtxstop (void);
void devtxstart (void);
void devtxinit (void);
void devtxflush (void);
void devtxput (uint8_t);
int32_t devtxwrite (uint8_t *, int32_t);
void devrxinit (void);
int32_t devrxavail (void);
int32_t devrxerror (void);
uint8_t devrxget (void);
void devinit (char *, int32_t, int32_t);
void devrestore (void);
void coninit (void);
void conrestore (void);
int32_t conget (void);

// file.c
void fileinit (void);
int32_t fileopen (char *, int32_t);
int32_t fileunit (int32_t);
int32_t fileseek (int32_t, int32_t, int32_t, int32_t);
int32_t fileread (int32_t, uint8_t *, int32_t);
int32_t filewrite (int32_t, uint8_t *, int32_t);
void fileclose (void);

// tu58drive.c
void tu58drive (void);


// Globals

extern uint8_t debug;
extern uint8_t verbose;
extern uint8_t nosync;
extern uint8_t timing;
extern uint8_t mrspen;
extern uint8_t vax;
extern uint8_t background;


// the end
