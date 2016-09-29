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
// TU58 File access routines
//



#include "common.h"



// file data structure

struct {
    int32_t	fd;		// file descriptor
    char	*name;		// file name
    uint8_t	rflag : 1;	// read allowed
    uint8_t	wflag : 1;	// write allowed
    uint8_t	cflag : 1;	// create allowed
    uint8_t	iflag : 1;	// init RT-11 structure
    uint8_t	xflag : 1;	// init XXDP structure
} file [NTU58];

int32_t fpt; // number of active file descriptors



//
// init file structures for all units
//
void fileinit (void)
{
    int32_t unit;

    for (unit = 0; unit < NTU58; unit++) {
	file[unit].fd = -1;
	file[unit].name = NULL;
	file[unit].rflag = 0;
	file[unit].wflag = 0;
	file[unit].cflag = 0;
	file[unit].iflag = 0;
	file[unit].xflag = 0;
    }
    fpt = 0;
    return;
}



//
// close file structures for all units
//
void fileclose (void)
{
    int32_t unit;

    for (unit = 0; unit < NTU58; unit++) {
	if (file[unit].fd != -1) {
	    close(file[unit].fd);
	    file[unit].fd = -1;
	}
    }
    return;
}



//
// init RT-11 file directory structures (based on RT-11 v5.4)
//
static int32_t rt11_init (int32_t fd)
{
    int32_t i;

    static int16_t boot[] = { // offset 0000000
 	0000240, 0000005, 0000404, 0000000, 0000000, 0041420, 0116020, 0000400,
	0004067, 0000044, 0000015, 0000000, 0005000, 0041077, 0047517, 0026524,
	0026525, 0067516, 0061040, 0067557, 0020164, 0067157, 0073040, 0066157,
	0066565, 0006545, 0005012, 0000200, 0105737, 0177564, 0100375, 0112037,
	0177566, 0100372, 0000777
    };

    static int16_t bitmap[] = { // offset 0001000
	0000000, 0170000, 0007777
    };

    static int16_t direct1[] = { // offset 0001700
	0177777, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000, 0000000,
	0000000, 0000001, 0000006, 0107123, 0052122, 0030461, 0020101, 0020040,
	0020040, 0020040, 0020040, 0020040, 0020040, 0020040, 0020040, 0020040,
	0042504, 0051103, 0030524, 0040461, 0020040, 0020040
    };

    static int16_t direct2[] = { // offset 0006000
	0000001, 0000000, 0000001, 0000000, 0000010, 0001000, 0000325, 0063471,
	0023364, 0000770, 0000000, 0002264, 0004000
    };

    static struct {
	int16_t *data;
	int16_t length;
	int32_t  offset;
    } table[] = {
	{ boot,    sizeof(boot),    00000 },
	{ bitmap,  sizeof(bitmap),  01000 },
	{ direct1, sizeof(direct1), 01700 },
	{ direct2, sizeof(direct2), 06000 },
	{ NULL, 0, 0 }
    };

    // now write data from the table
    for (i = 0; table[i].length; i++) {
	lseek(fd, table[i].offset, SEEK_SET);
	if (write(fd, table[i].data, table[i].length) != table[i].length) return -1;
    }

    return 0;
}



//
// init XXDP file directory structures (based on XXDPv2.5)
//
static int32_t xxdp_init (int32_t fd)
{
    int32_t i;

    static int16_t mfd1[] = { // MFD1
	0000002, // ptr to MFD2
	0000001, // interleave factor
	0000007, // BITMAP start block number
	0000007  // ptr to 1st BITMAP block
    };

    static int16_t mfd2[] = { // MFD2
	0000000, // no more MFDs
	0000401, // uic [1,1]
	0000003, // ptr to 1st UFD block
	0000011  // 9. words per UFD entry
    };

    static int16_t ufd1[] = { // UFD#1 (empty directory)
	0000004  // ptr to UFD#2
    };

    static int16_t ufd2[] = { // UFD#2 (empty directory)
	0000005  // ptr to UFD#3
    };

    static int16_t ufd3[] = { // UFD#3 (empty directory)
	0000006  // ptr to UFD#4
    };

    static int16_t ufd4[] = { // UFD#4 (empty directory)
	0000000  // no more UFDs
    };

    static int16_t map1[] = { // BITMAP#1
	0000000, // no more BITMAPs
	0000001, // map number
	0000074, // 60. words per BITMAP
	0000007, // ptr to BITMAP#1
	0177777, // blocks 15..00 allocated
	0177777, // blocks 31..16 allocated
	0000377  // blocks 39..32 allocated
    };

    static struct {
	int16_t *data;
	int16_t length;
	int32_t  offset;
    } table[] = {
	{ mfd1, sizeof(mfd1), 01000 },
	{ mfd2, sizeof(mfd2), 02000 },
	{ ufd1, sizeof(ufd1), 03000 },
	{ ufd2, sizeof(ufd2), 04000 },
	{ ufd3, sizeof(ufd3), 05000 },
	{ ufd4, sizeof(ufd4), 06000 },
	{ map1, sizeof(map1), 07000 },
	{ NULL, 0, 0 }
    };

    // now write data from the table
    for (i = 0; table[i].length; i++) {
	lseek(fd, table[i].offset, SEEK_SET);
	if (write(fd, table[i].data, table[i].length) != table[i].length) return -1;
    }

    return 0;
}



//
// init a blank tape image (all zero)
//
static int32_t zero_init (int32_t fd)
{
    int32_t i;
    int8_t buf[BLOCKSIZE];

    // zero a block
    memset(buf, 0, sizeof(buf));

    // zero a whole tape
    lseek(fd, 0, SEEK_SET);
    for (i = 0; i < TAPESIZE; i++)
	if (write(fd, buf, sizeof(buf)) != sizeof(buf)) return -1;

    return 0;
}



//
// open a file for a unit
//
int32_t fileopen (char *name,
		  int32_t mode)
{
    int32_t fd;

    // check if we can open any more units
    if (fpt >= NTU58) { error("no more units available"); return -1; }

    // save some data
    file[fpt].name = name;
    file[fpt].rflag = 1;
    if (mode == FILEWRITE) file[fpt].wflag = 1;
    if (mode == FILECREATE) file[fpt].wflag = file[fpt].cflag = 1;
    if (mode == FILERT11INIT) file[fpt].wflag = file[fpt].cflag = file[fpt].iflag = 1;
    if (mode == FILEXXDPINIT) file[fpt].wflag = file[fpt].cflag = file[fpt].xflag = 1;

    // open file if it exists
    if (file[fpt].wflag)
	fd = open(file[fpt].name, O_BINARY|O_RDWR, 0666);
    else
	fd = open(file[fpt].name, O_BINARY|O_RDONLY);

    // create file if it does not exist
    if (fd < 0 && file[fpt].cflag) fd = creat(file[fpt].name, 0666);
    if (fd < 0) { error("fileopen cannot open or create '%s'", file[fpt].name); return -2; }

    // store opened file information
    file[fpt].fd = fd;

    // zap tape if requested
    if (file[fpt].cflag) {
	if (!zero_init(fd)) {
	    info("initialize tape on '%s'", file[fpt].name);
	} else {
	    error("fileopen cannot init tape on '%s'", file[fpt].name);
	    return -3;
	}
    }

    // initialize RT-11 directory structure ?
    if (file[fpt].iflag) {
	if (!rt11_init(fd)) {
	    info("initialize RT-11 directory on '%s'", file[fpt].name);
	} else {
	    error("fileopen cannot init RT-11 filesystem on '%s'", file[fpt].name);
	    return -4;
	}
    }

    // initialize XXDP directory structure ?
    if (file[fpt].xflag) {
	if (!xxdp_init(fd)) {
	    info("initialize XXDP directory on '%s'", file[fpt].name);
	} else {
	    error("fileopen cannot init XXDP filesystem on '%s'", file[fpt].name);
	    return -5;
	}
    }

    // output some info...
    info("unit %d %c%c%c%c file '%s'",
	 fpt,
	 file[fpt].rflag ? 'r' : ' ',
	 file[fpt].wflag ? 'w' : ' ',
	 file[fpt].cflag ? 'c' : ' ',
	 file[fpt].iflag ? 'i' : file[fpt].xflag ? 'x' : ' ',
	 file[fpt].name);

    fpt++;
    return 0;
}



//
// check file unit OK
//
int32_t fileunit (int32_t unit)
{
    if (unit < 0 || unit >= NTU58 || file[unit].fd == -1) {
	error("bad unit %d", unit);
	return -1;
    }
    return 0;
}



//
// seek file (tape) position
//
int32_t fileseek (int32_t unit,
		  int32_t size,
		  int32_t block,
		  int32_t offset)
{
    if (fileunit(unit)) return -1;

    if (block*size+offset > lseek(file[unit].fd, 0, SEEK_END)) return -2;

    if (lseek(file[unit].fd, block*size+offset, SEEK_SET) < 0) return -3;

    return 0;
}



//
// read bytes from the tape image file
//
int32_t fileread (int32_t unit,
		  uint8_t *buffer,
		  int32_t count)
{
    if (fileunit(unit)) return -1;

    if (!file[unit].rflag) return -2;

    return read(file[unit].fd, buffer, count);
}



//
// write bytes to the tape image file
//
int32_t filewrite (int32_t unit,
		   uint8_t *buffer,
		   int32_t count)
{
    if (fileunit(unit)) return -1;

    if (!file[unit].wflag) return -2;

    return write(file[unit].fd, buffer, count);
}



// the end
