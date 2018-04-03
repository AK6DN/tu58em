//
// tu58 - Emulate a TU58 over a serial line
//
// Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
// Update   (C) 2005-2017 Donald N North <ak6dn_at_mindspring_dot_com>
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
// TU58 serial support routines
//



#include "common.h"

#ifdef WINCOMM
#include <windef.h>
#include <winbase.h>
#endif // WINCOMM

#ifdef MACOSX
#define IUCLC 0 // Not POSIX
#define OLCUC 0 // Not POSIX
#define CBAUD 0 // Not POSIX
#endif // MACOSX

#include <termios.h>

#define	BUFSIZE	256	// size of serial line buffers (bytes, each way)

// serial output buffer
static uint8_t  wbuf[BUFSIZE];
static uint8_t *wptr;
static int32_t  wcnt;

// serial input buffer
static uint8_t  rbuf[BUFSIZE];
static uint8_t *rptr;
static int32_t  rcnt;

#ifdef WINCOMM
// serial device descriptor, default to nada
static HANDLE hDevice = INVALID_HANDLE_VALUE;
// async line parameters
static DCB dcbSave;
static COMMTIMEOUTS ctoSave;
static uint8_t rxBreakSeen;
#else // !WINCOMM
// serial device descriptor, default to nada
static int32_t device = -1;
// async line parameters
static struct termios lineSave;
#endif // !WINCOMM

// console parameters
static struct termios consSave;



#ifdef WINCOMM
//
// delay routine
//
static void delay_ms (int32_t ms)
{
    struct timespec rqtp;
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
#endif // WINCOMM



//
// stop transmission on output
//
void devtxstop (void)
{
#ifdef WINCOMM
    if (!EscapeCommFunction(hDevice, SETXOFF))
	error("devtxstop(): error=%d", GetLastError());
#else // !WINCOMM
    tcflow(device, TCOOFF);
#endif // !WINCOMM
    return;
}



//
// (re)start transmission on output
//
void devtxstart (void)
{
#ifdef WINCOMM
    if (!EscapeCommFunction(hDevice, SETXON))
	error("devtxstart(): error=%d", GetLastError());
#else // !WINCOMM
    tcflow(device, TCOON);
#endif // !WINCOMM
    return;
}



//
// set/clear break condition on output
//
void devtxbreak (void)
{
#ifdef WINCOMM
    if (!SetCommBreak(hDevice))
	error("devtxbreak(set): error=%d", GetLastError());
    delay_ms(250);
    if (!ClearCommBreak(hDevice))
	error("devtxbreak(clear): error=%d", GetLastError());
#else // !WINCOMM
    tcsendbreak(device, 0);
#endif // !WINCOMM
    return;
}



//
// initialize tx serial buffers
//
void devtxinit (void)
{
    // flush all output
#ifdef WINCOMM
    if (!PurgeComm(hDevice, PURGE_TXABORT|PURGE_TXCLEAR))
	error("devtxinit(): error=%d", GetLastError());
#else // !WINCOMM
    tcflush(device, TCOFLUSH);
#endif // !WINCOMM

    // reset send buffer
    wcnt = 0;
    wptr = wbuf;

    return;
}



//
// initialize rx serial buffers
//
void devrxinit (void)
{
    // flush all input
#ifdef WINCOMM
    if (!PurgeComm(hDevice, PURGE_RXABORT|PURGE_RXCLEAR))
	error("devrxinit(): error=%d", GetLastError());
    rxBreakSeen = 0;
#else // !WINCOMM
    tcflush(device, TCIFLUSH);
#endif // !WINCOMM

    // reset receive buffer
    rcnt = 0;
    rptr = rbuf;

    return;
}



//
// return number of characters available, get more if receive buffer is empty
//
int32_t devrxavail (void)
{
    // get more characters if none available
    if (rcnt <= 0) {
#ifdef WINCOMM
	COMSTAT stat;
	DWORD acnt = 0;
	DWORD ncnt = 0;
	DWORD sts = 0;
	// clear state
	if (!ClearCommError(hDevice, &sts, &stat))
	    error("devrxavail(): ClearCommError() failed");
	// do the read if something there, at most size of buffer
	ncnt = stat.cbInQue > sizeof(rbuf) ? sizeof(rbuf) : stat.cbInQue;
	if (!ReadFile(hDevice, rbuf, ncnt, &acnt, NULL))
	    error("devrxavail(): error=%d", GetLastError());
	// check for break
	if (sts & CE_BREAK) rxBreakSeen = 1;
	// done
	rcnt = acnt;
#else // !WINCOMM
	rcnt = read(device, rbuf, sizeof(rbuf));
#endif // !WINCOMM
	rptr = rbuf;
    }
    if (rcnt < 0) rcnt = 0;

    // return characters available
    return rcnt;
}



//
// write characters direct to device from transmit buffer
//
int32_t devtxwrite (uint8_t *buf,
		    int32_t cnt)
{
    // write characters if asked, return number written
    if (cnt > 0) {
#ifdef WINCOMM
	COMSTAT stat;
	DWORD acnt = 0;
	DWORD sts = 0;
	// clear state
	if (!ClearCommError(hDevice, &sts, &stat))
	    error("devtxwrite(): ClearCommError() failed");
	// do the write
	if (!WriteFile(hDevice, buf, cnt, &acnt, NULL))
	    error("devtxwrite(): error=%d", GetLastError());
	// done
	return acnt;
#else // !WINCOMM
	return write(device, buf, cnt);
#endif // !WINCOMM
    }
    // nothing done if we got here
    return 0;
}



//
// send any outgoing characters in buffer
//
void devtxflush (void)
{
    int32_t acnt;
    
    // write any characters we have
    if (wcnt > 0) {
	if ((acnt = devtxwrite(wbuf, wcnt)) != wcnt)
	    error("devtxflush(): write error, expected=%d, actual=%d", wcnt, acnt);
    }

    // buffer is now empty
    wcnt = 0;
    wptr = wbuf;

    // wait until all characters are transmitted
#ifdef WINCOMM
    if (!FlushFileBuffers(hDevice))
	error("devtxflush(): FlushFileBuffers() failed, error=%d", GetLastError());
#else // !WINCOMM
    tcdrain(device);
#endif // !WINCOMM

    return;
}



//
// return char from rbuf, wait until some arrive
//
uint8_t devrxget (uint8_t *flg)
{
    uint8_t c;

#ifdef USE_PARMRK
    // get more bytes if none available
    while (rcnt <= 0) { (void)devrxavail(); }
    // at least one available
    rcnt--;
    // check if escaped or normal
    if ((c = *rptr++) == 0377) {
        // escape byte seen
        while (rcnt <= 0) { (void)devrxavail(); }
        // at least one available
        rcnt--;
        // check if escape or not
        if ((c = *rptr++) == 0377) {
            // 377,377 seen; return 377
            *flg = DEV_NORMAL;
            return c;
        } else {
            // non-escape byte seen, so get one more byte
            while (rcnt <= 0) { (void)devrxavail(); }
            // at least one available
            rcnt--;
            // check if NULL
            if ((c = *rptr++) == 0000) {
                // 377,000,000 seen; signals a BREAK, return 000 byte
                *flg = DEV_BREAK;
                return c;
            } else {
                // 377,000,NNN seen; signals byte NNN parity/framing error
                *flg = DEV_ERROR;
                return c;
            }
        }
    } else {
        // return normal data byte
        *flg = DEV_NORMAL;
        return c;
    }
#else // !USE_PARMRK
    // get more characters if none available
    while (rcnt <= 0) { (void)devrxavail(); }
    // at least one available
    rcnt--;
    // get data byte
    c = *rptr++;
#ifdef WINCOMM
    // check if this byte should be flagged as a BREAK
    // for lack of a better algorithm, we flag the first
    // ZERO byte after the rxBreakSeen flag is set as BREAK
    if (c == 000 && rxBreakSeen) {
	*flg = DEV_BREAK;
	rxBreakSeen = 0;
    } else {
	*flg = DEV_NORMAL;
    }
#else // !WINCOMM
    // return normal data byte
    *flg = DEV_NORMAL;
#endif // !WINCOMM
    // return data byte
    return c;
#endif // !USE_PARMRK
}



//
// put char on wbuf
//
void devtxput (uint8_t c)
{

    // must flush if hit the end of the buffer
    if (wcnt >= sizeof(wbuf)) devtxflush();

    // count, add one character to buffer
    wcnt++;
    *wptr++ = c;
    return;
}



//
// return baud rate mask for a given rate
//
static int32_t devbaud (int32_t rate)
{
#ifdef WINCOMM
    static int32_t baudlist[] = {
	3000000, 3000000,
	2500000, 2500000,
	2000000, 2000000,
	1500000, 1500000,
	1152000, 1152000,
	1000000, 1000000,
	921600,  921600,
	576000,  576000,
	500000,  500000,
	460800,  460800,
	230400,  230400,
	115200,  115200,
	57600,   57600,
	38400,   38400,
	19200,   19200,
	9600,    9600,
	4800,    4800,
	2400,    2400,
	1200,    1200,
	-1,	   -1
    };
#else // !WINCOMM
    static int32_t baudlist[] = {
#ifdef B3000000
	3000000, B3000000,
#endif // B3000000
#ifdef B2500000
	2500000, B2500000,
#endif // B2500000
#ifdef B2000000
	2000000, B2000000,
#endif // B2000000
#ifdef B1500000
	1500000, B1500000,
#endif // B1500000
#ifdef B1152000
	1152000, B1152000,
#endif // B1152000
#ifdef B1000000
	1000000, B1000000,
#endif // B1000000
#ifdef B921600
	921600,  B921600,
#endif // B921600
#ifdef B576000
	576000,  B576000,
#endif // B576000
#ifdef B500000
	500000,  B500000,
#endif // B500000
#ifdef B460800
	460800,  B460800,
#endif // B460800
#ifdef B230400
	230400,  B230400,
#endif // B230400
#ifdef B115200
	115200,  B115200,
#endif // B115200
#ifdef B57600
	57600,   B57600,
#endif // B57600
#ifdef B38400
	38400,   B38400,
#endif // B38400
#ifdef B19200
	19200,   B19200,
#endif // B19200
#ifdef B9600
	9600,    B9600,
#endif // B9600
#ifdef B4800
	4800,    B4800,
#endif // B4800
#ifdef B2400
	2400,    B2400,
#endif // B2400
#ifdef B1200
	1200,    B1200,
#endif // B1200
	-1,	    -1
    };
#endif // !WINCOMM
    int32_t *p = baudlist;
    int32_t r;

    // search table for a baud rate match, return corresponding entry
    while ((r = *p++) != -1) if (r == rate) return *p; else p++;

    // not found ...
    return -1;
}



//
// open/initialize serial port
//
void devinit (char *port,
	      int32_t speed,
	      int32_t stop)
{
#ifdef WINCOMM

    // init win32 serial port mode
    DCB dcb;
    COMMTIMEOUTS cto;
    char name[64];
    int32_t n;

    // open serial port
    int32_t euid = geteuid();
    int32_t uid = getuid();
    setreuid(euid, -1);
    if (sscanf(port, "%u", &n) == 1) sprintf(name, "\\\\.\\COM%d", n); else strcpy(name, port);
    // open port in non-overlapped I/O mode
    hDevice = CreateFile(name,
			 GENERIC_READ | GENERIC_WRITE,
			 0,
			 NULL,
			 OPEN_EXISTING,
			 FILE_ATTRIBUTE_NORMAL,
			 NULL);
    if (hDevice == INVALID_HANDLE_VALUE) fatal("no serial line [%s]", name);

    // we own the port
    setreuid(uid, euid);

    // get current line params, error if not a serial port
    if (!GetCommState(hDevice, &dcbSave)) fatal("GetCommState() failed");
    if (!GetCommTimeouts(hDevice, &ctoSave)) fatal("GetCommTimeouts() failed");

    // copy current parameters
    dcb = dcbSave;
    cto = ctoSave;

    // set baud rate
    if (devbaud(speed) == -1)
	error("illegal serial speed %d., ignoring", speed);
    else
	dcb.BaudRate = devbaud(speed);

    // update other line parameters
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fInX = FALSE;
    dcb.fOutX = FALSE;
    dcb.fNull = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fAbortOnError = FALSE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = (stop == 2) ? TWOSTOPBITS : ONESTOPBIT;

    // timing/read param
    cto.ReadIntervalTimeout = MAXDWORD;
    cto.ReadTotalTimeoutMultiplier = 0;
    cto.ReadTotalTimeoutConstant = 0;
    cto.WriteTotalTimeoutMultiplier = 0;
    cto.WriteTotalTimeoutConstant = 0;

    // ok, set new param
    if (!SetupComm(hDevice, BUFSIZE, BUFSIZE)) fatal("SetupComm() failed");
    if (!SetCommState(hDevice, &dcb)) fatal("SetCommState() failed");
    if (!SetCommTimeouts(hDevice, &cto)) fatal("SetCommTimeouts() failed");

#else // !WINCOMM

    // init unix serial port mode
    struct termios line;
    char name[64];
    unsigned int n;

    // open serial port
    int32_t euid = geteuid();
    int32_t uid = getuid();
    if (setreuid(euid, -1)) fatal("setreuid(euid,-1) failed");
    if (sscanf(port, "%u", &n) == 1) sprintf(name, "/dev/ttyS%u", n-1); else strcpy(name, port);
    if ((device = open(name, O_RDWR|O_NDELAY|O_NOCTTY)) < 0) fatal("no serial line [%s]", name);
    if (setreuid(uid, euid)) fatal("setreuid(uid,euid) failed");

    // get current line params, error if not a serial port
    if (tcgetattr(device, &lineSave)) fatal("not a serial device [%s]", name);

    // copy current parameters
    line = lineSave;

    // input param
    line.c_iflag &= ~( IGNBRK | BRKINT | IMAXBEL | INPCK | ISTRIP |
		       INLCR  | IGNCR  | ICRNL   | IXON  | IXOFF  |
		       IUCLC  | IXANY  | PARMRK  | IGNPAR );
#ifdef USE_PARMRK
    line.c_iflag |=  ( PARMRK | INPCK );
#else // !USE_PARMRK
    line.c_iflag |=  ( 0 );
#endif // !USE_PARMRK

    // output param
    line.c_oflag &= ~( OPOST  | OLCUC | OCRNL | ONLCR | ONOCR |
		       ONLRET | OFILL | CRDLY | NLDLY | BSDLY |
		       TABDLY | VTDLY | FFDLY | OFDEL );
    line.c_oflag |=  ( 0 );

    // control param
    line.c_cflag &= ~( CBAUD  | CSIZE | CSTOPB  | PARENB | PARODD |
		       HUPCL | CRTSCTS | CLOCAL | CREAD );
    line.c_cflag |=  ( CLOCAL | CREAD | CS8 );

    // set two stop bits if requested, else default to one
    if (stop == 2) line.c_cflag |= CSTOPB;

    // local param
    line.c_lflag &= ~( ISIG   | ICANON  | ECHO   | ECHOE  | ECHOK  |
		       ECHONL | NOFLSH  | TOSTOP | IEXTEN | FLUSHO |
		       ECHOKE | ECHOCTL );
    line.c_lflag |=  ( 0 );

    // timing/read param
    line.c_cc[VMIN] = 1; // return a min of 1 chars
    line.c_cc[VTIME] = 0; // no timer

    // flush all existing input data
    tcflush(device, TCIFLUSH);

    // set baud rate, if it is legal
    if (devbaud(speed) == -1) {
	error("illegal serial speed %d., ignoring", speed);
    } else {
	cfsetispeed(&line, devbaud(speed));
	cfsetospeed(&line, devbaud(speed));
    }

    // set new device parameters
    tcsetattr(device, TCSANOW, &line);

    // and non-blocking also
    if (fcntl(device, F_SETFL, FNDELAY) == -1)
	error("failed to set non-blocking read");

#endif // !WINCOMM

    // zap current data, if any
    devtxinit();
    devrxinit();

    return;
}



//
// restore/close serial port
//
void devrestore (void)
{
#ifdef WINCOMM
    if (!CloseHandle(hDevice))
	error("devrestore(): error=%d", GetLastError());
    hDevice = INVALID_HANDLE_VALUE;
#else // !WINCOMM
    tcsetattr(device, TCSANOW, &lineSave);
    close(device);
    device = -1;
#endif // !WINCOMM
    return;
}



//
// set console line parameters
//
void coninit (void)
{
    struct termios cons;

    // background mode, don't do anything
    if (background) return;

    // get current console parameters
    if (tcgetattr(fileno(stdin), &consSave))
	fatal("stdin not a serial device");

    // copy modes
    cons = consSave;

    // set new modes
    cons.c_lflag &= ~( ICANON | ECHO );

    // now set param
    tcflush(fileno(stdin), TCIFLUSH);
    tcsetattr(fileno(stdin), TCSANOW, &cons);

    // set non-blocking reads
    if (fcntl(fileno(stdin), F_SETFL, FNDELAY) == -1)
	error("stdin failed to set non-blocking read");

    return;
}



//
// restore console line parameters
//
void conrestore (void)
{
    // background mode, don't do anything
    if (background) return;

    // restore console mode to saved
    tcsetattr(fileno(stdin), TCSANOW, &consSave);
    return;
}



//
// get console character
//
int32_t conget (void)
{
    char buf[1];
    int32_t s;

    // background mode, don't return anything
    if (background) return -1;

    // try to read at most one char (may be none)
    s = read(fileno(stdin), buf, sizeof(buf));

    // if got a char return it, else return -1
    return s == 1 ? *buf : -1;
}



// the end
