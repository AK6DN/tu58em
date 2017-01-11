<B>tu58em</B> is a software emulation of a DECtapeII TU-58 block addressable cartridge tape drive. It requires only a standard Windows PC as a host with an RS232 serial port to connect to the target system.

<B>tu58em</B> was originally based on the 1984 Dan Ts'o tu58 program, but has been almost completely rewritten to make it compile error free, improve the program flow, and add new functionality. It has been compiled within the CYGWIN environment, and will run either within a CYGWIN window or an MSDOS command window with the associated cygwin1.dll helper file. <B>tu58em</B> has been tested under Win2K, WinXPsp3, and Win7sp1. <B>tu58em</B> has compiled error free under Linux (Ubuntu 12.02LTS) but has not yet been rigorously tested in that environment.

Each emulated .dsk image file is exactly 256KB (512 blocks of 512 bytes) of data and is a byte-for-byte image of the data on a TU-58 cartridge tape. As currently configured <B>tu58em</B> will support up to 8 drives per controller as DD0: to DD7: (altho this is easily changed in the source). <B>tu58em</B> will support disk image files as large as the TU58 protocol allows (ie, 32MB, or 64K 512B blocks); however most standard DEC operating system drivers restrict TU58 drives to 256KB each. The DEC driver must be patched to allow for larger than 256KB disk images.

<B>tu58em</B> has been tested using both native RS232 serial COM ports and serial ports emulated thru USB serial adapters.

Note: <B>tu58em</B> is compiled using the CYGWIN serial interface library routines (ie, termios.h) and is the preferred version that should be used. <B>tu58ew</B> bypasses the CYGWIN serial interface layer and makes direct Windows serial comm routine calls. This version was done early on because of deficiencies in the CYGWIN serial library. However, the required features are now present so in reality <B>tu58ew</B> should be considered deprecated.

As of v1.4m Mark Blair's updates for VAX mode operation (for VAX-730 microcode boot support) and background mode are integrated.

A cygwin folder with a precompiled 32b cygwin executable (tu58em.exe) is included for those without cygwin environment access. Under Windows, just open a standard CMD.EXE window, change to the cygwin folder, and run the <B>tu58em.exe</B> executable as a command line program.

If the emulator is run with no options, it prints a usage screen:

```
E:\DEC> tu58em
ERROR: no units were specified
FATAL: illegal command line
  tu58 tape emulator v1.4o
  Usage: ./tu58em [-options] -[rwci] file1 ... -[rwci] file7
  Options: -V | --version            output version string
           -v | --verbose            enable verbose output to terminal
           -d | --debug              enable debug output to terminal
           -m | --mrsp               enable standard MRSP mode (byte-level handshake)
           -n | --nosync             disable sending INIT at initial startup
           -x | --vax                remove delays for aggressive timeout of VAX console
           -b | --background         run in background mode, no console I/O except errors
           -t | --timing 1           add timing delays to spoof diagnostic into passing
           -T | --timing 2           add timing delays to mimic a real TU58
           -s | --speed BAUD         set line speed 1200..3000000; default 9600
           -S | --stop BITS          set stop bits 1..2; default 1
           -p | --port PORT          set port to PORT [1..N or /dev/comN; default 1]
           -r | --read|rd FILENAME   readonly drive
           -w | --write FILENAME     read/write drive
           -c | --create FILENAME    create new r/w drive, zero tape
           -i | --initrt11 FILENAME  create new r/w drive, initialize RT11 directory
           -z | --initxxdp FILENAME  create new r/w drive, initialize XXDP directory
E:\DEC>
```

Most of the switches should be pretty obvious:

```
-V   prints the program version and exits
-v   sets verbose mode, which outputs status as the emulator runs
-d   sets debug mode, which dumps out all packets sent/received
-m   enables MRSP mode (VERY MUCH UNTESTED) instead of the default original RSP mode
-n   disables the sending of INIT characters at startup
-x   remove delays for aggressive timeout of VAX console
-b   run in background mode, no console I/O except errors
-t   adds time delays to allow the emulator to pass the DEC ZTUUF0 TU-58 Performance Exerciser diagnostic
-T   adds time delays to make the emulator nearly as slow as a real TU-58 (just for fun)
-s BAUD      sets the baud rate; the following rates are supported. the default will be 9600 if not set.
                  3000000, 2500000, 2000000, 1500000, 1152000, 1000000, 921600, 576000, 500000,
                  460800, 230400, 115200, 57600, 38400, 19200, 9600, 4800, 2400, 1200
-p PORT      sets the com port as a number (1,2,3,...) or if not numeric the full path (/dev/com1)
-S STOP      sets the number of stop bits (1 or 2), default is 1
-r FILENAME  set the next unit as a read only drive using file FILENAME
-w FILENAME  set the next unit as a read/write drive using file FILENAME
-c FILENAME  set the next unit as a read/write drive using file FILENAME, zero the file before use
-i FILENAME  set the next unit as a read/write drive using file FILENAME, initialize RT-11 filesystem before use
-z FILENAME  set the next unit as a read/write drive using file FILENAME, initialize XXDP filesystem before use
```

A sample run of <B>tu58em</B>, using COM3 at 38.4Kb, a read/only tape on DD0: using file boot.dsk, and a read/write tape on DD1: initialized with an RT-11 filesystem as file rt11.dsk:

```
E:\DEC> tu58em -p 3 -s 38400 -r boot.dsk -i rt11.dsk
info: initialize RT-11 directory on 'rt11.dsk'
info: unit 0 r    file 'boot.dsk'
info: unit 1 rwci file 'rt11.dsk'
info: serial port 3 at 38400 baud 1 stop
info: TU58 emulation start
info: R restart, S toggle send init, V toggle verbose, D toggle debug, Q quit
info: emulator started
info: <INIT><INIT> seen, sending <CONT>
info: read     unit=0 sw=0x00 mod=0x00 blk=0x0000 cnt=0x0400
info: read     unit=0 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
info: read     unit=0 sw=0x00 mod=0x00 blk=0x0001 cnt=0x0200
info: read     unit=0 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
info: read     unit=1 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
info: read     unit=1 sw=0x00 mod=0x00 blk=0x0001 cnt=0x0200
info: read     unit=1 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
info: write    unit=1 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
info: write    unit=1 sw=0x00 mod=0x00 blk=0x0001 cnt=0x0200
info: write    unit=1 sw=0x00 mod=0x00 blk=0x0000 cnt=0x0046
info: read     unit=0 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
info: write    unit=1 sw=0x00 mod=0x00 blk=0x0006 cnt=0x0400
  [ Q typed ]
info: TU58 emulation end
E:\DEC>
```

