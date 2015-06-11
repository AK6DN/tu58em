//
// tu58 - Emulate a TU58 over a serial line
//
// Original (C) 1984 Dan Ts'o <Rockefeller Univ. Dept. of Neurobiology>
// Update (C) 2005-2012 Don North <ak6dn_at_mindspring_dot_com>
//
// This is the TU58 emulation program written at Rockefeller Univ., Dept. of
// Neurobiology. We copyright (C) it and permit its use provided it is not
// sold to others. Originally written by Dan Ts'o circa 1984 or so.
//
// Extensively rewritten to work reliably under the CYGWIN unix subsystem.
// Has been tested up to 38.4Kbaud hosted on a 1GHz Pentium3 Win2K platform.
// Added native windows serial comm support compile option (for break detect).
// Added multithreaded supervisor for command parser and emulator restart.
// Added timing delay option to allow emulator to pass diagnostic ZTUUF0.BIN.
// Supports four (or more, compile option) logical TU58 devices per controller.
//
// v1.0b - 12 Jul 2005 - donorth - Initial rewrite
// v1.1b - 20 Feb 2006 - donorth - Fixed RT11 f/s init; added XXDP f/s init
// v1.2b - 25 Feb 2006 - donorth - Updated -p arg to be number or string
//                               - Fixed some typos in help string
// v1.3b - 28 Feb 2006 - donorth - Updated windoze serial line config
// v1.4b - 25 May 2006 - donorth - Send INITs on initialization/restart
// v1.4c - 25 Nov 2007 - donorth - Make <INIT><INIT> message be debug, not verbose
//                               - Fix fileseek() to correctly detect LEOF
//                               - read/write no longer skip past LEOF
// v1.4d - 10 Feb 2008 - donorth - added XRSP switch for packet-level handshake
// v1.4e - 16 Feb 2008 - donorth - changed to --long switches from -s
//                               - Slightly updated RT-11 disk init bits
//                               - Added --nosync switch
// v1.4f - 02 Jan 2012 - donorth - Restructuring for ultimate inclusion
//                                 of a tu58 controller implementation
//                                 (ie, to hook to a real tu58 drive or the emulator)
// v1.4g - 30 Oct 2013 - donorth - Removed XRSP switch
// v1.4h - 03 Nov 2013 - donorth - Added packet timing computation in debug mode
// v1.4i - 14 Nov 2013 - donorth - Disabled devtxflush() in wait4cont() routine
//                                 to fix USB output (PC to device) thruput and
//                                 allow 64B USB packets to be used
// v1.4j - 01 Nov 2014 - donorth - Change 'int' to 'long' where possible.
//                                 Only use char/short/long types, not int.
//                                 Fix source for ubuntu linux 12.04 (time structs)
// v1.4k - 11 Jun 2015 - donorth - Integrate Mark Blair's changes for MacOSX compilation
//                               - No functionality changes on other platforms
//


#include "common.h"
#include <getopt.h>


static char copyright[] = "(C) 2005-2014 Don North <ak6dn" "@" "mindspring.com>, " \
                          "(C) 1984 Dan Ts'o <Rockefeller University>";

static char version[] = "tu58 tape emulator v1.4k";

static char port[32] = "1"; // default port number (COM1, /dev/ttyS0)
static long speed = 9600; // default line speed

uint8_t verbose = 0; // set nonzero to output more info
uint8_t timing = 0; // set nonzero to add timing delays
uint8_t mrspen = 0; // set nonzero to enable MRSP mode
uint8_t nosync = 0; // set nonzero to skip sending INIT at restart
uint8_t debug = 0; // set nonzero for debug output



//
// print an info message and return
//
void info (char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "info: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    return;
}



//
// print an error message and return
//
void error (char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "ERROR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    return;
}



//
// print an error message and die
//
void fatal (char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}



//
// main program
//
int main (int argc,
	  char **argv)
{
    long i;
    long n = 0;
    long errors = 0;

    // switch options
    int opt_index = 0;
    char opt_short[] = "dvVmnTtp:s:r:w:c:i:z:";
    static struct option opt_long[] = {
	{ "debug",	no_argument,       0, 'd' },
	{ "verbose",	no_argument,       0, 'v' },
	{ "version",	no_argument,       0, 'V' },
	{ "mrsp",	no_argument,       0, 'm' },
	{ "nosync",	no_argument,       0, 'n' },
	{ "timing",	required_argument, 0, -2  },
	{ "port",	required_argument, 0, 'p' },
	{ "baud",	required_argument, 0, 's' },
	{ "speed",	required_argument, 0, 's' },
	{ "rd",		required_argument, 0, 'r' },
	{ "read",	required_argument, 0, 'r' },
	{ "write",	required_argument, 0, 'w' },
	{ "create",	required_argument, 0, 'c' },
	{ "initrt11",	required_argument, 0, 'i' },
	{ "initxxdp",	required_argument, 0, 'z' },
	{  0,           no_argument,       0,  0  }
    };

    // init file structures
    fileinit();

    // process command line options
    while ((i = getopt_long(argc, argv, opt_short, opt_long, &opt_index)) != EOF) {
	switch (i) {
	case -2 :  timing = atoi(optarg); if (timing > 2) errors++; break;
	case 'p':  strcpy(port, optarg);  break;
	case 's':  speed = atoi(optarg);  break;
	case 'r':  fileopen(optarg, FILEREAD);  n++;  break;
	case 'w':  fileopen(optarg, FILEWRITE);  n++;  break;
	case 'c':  fileopen(optarg, FILECREATE);  n++;  break;
	case 'i':  fileopen(optarg, FILERT11INIT);  n++;  break;
	case 'z':  fileopen(optarg, FILEXXDPINIT);  n++;  break;
	case 'm':  mrspen = 1;  break;
	case 'n':  nosync = 1;  break;
	case 'T':  timing = 2;  break;
	case 't':  timing = 1;  break;
	case 'd':  verbose = 1; debug = 1;  break;
	case 'v':  verbose = 1;  break;
	case 'V':  info("version is %s", version);  break;
	default:   errors++; break;
	}
    }

    // some debug info
    if (debug) { info(version); info(copyright); }

    // must have opened at least one unit
    if (n == 0) {
	error("no units were specified");
	errors++;
    }

    // any error seen, die and print out some help
    if (errors)
	fatal("illegal command line\n" \
	      "  %s\n" \
	      "  Usage: %s [-options] -[rwci] file1 ... -[rwci] file%d\n" \
	      "  Options: -V | --version            output version string\n" \
	      "           -v | --verbose            enable verbose output to terminal\n" \
	      "           -d | --debug              enable debug output to terminal\n" \
	      "           -m | --mrsp               enable standard MRSP mode (byte-level handshake)\n" \
	      "           -n | --nosync             disable sending INIT at initial startup\n" \
	      "           -t | --timing 1           add timing delays to spoof diagnostic into passing\n" \
	      "           -T | --timing 2           add timing delays to mimic a real TU58\n" \
	      "           -s | --speed BAUD         set line speed [1200..230400; default 9600]\n" \
	      "           -p | --port PORT          set port to PORT [1..N or /dev/comN; default 1]\n" \
	      "           -r | --read|rd FILENAME   readonly drive\n" \
	      "           -w | --write FILENAME     read/write drive\n" \
	      "           -c | --create FILENAME    create new r/w drive, zero tape\n" \
	      "           -i | --initrt11 FILENAME  create new r/w drive, initialize RT11 directory\n" \
	      "           -z | --initxxdp FILENAME  create new r/w drive, initialize XXDP directory\n",
	      version, argv[0], NTU58-1);

    // give some info
    info("serial port %s at %d baud", port, speed);
    if (mrspen) info("MRSP mode enabled (NOT fully tested - use with caution)");

    // setup serial and console ports
    devinit(port, speed);
    coninit();
    
    // play TU58
    tu58drive();

    // restore serial and console ports
    conrestore();
    devrestore();

    // close files we opened
    fileclose();

    // and done
    return EXIT_SUCCESS;
}



// the end
