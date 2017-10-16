#
# tu58em emulator makefile
#

# get operating system name
OPSYS = $(shell uname -s)

ifeq ($(OPSYS),Darwin)
# mac: UNIX comms model, but on MACOSX
OPTIONS = -DMACOSX
LFLAGS = -lpthread
BINDIR = /usr/local/bin
else ifeq ($(OPSYS:CYGWIN%=CYGWIN),CYGWIN)
# win: WINDOWS comms model under CYGWIN (any version)
OPTIONS = -DCYGWIN -DWINCOMM
LFLAGS = -lpthread -lrt
BINDIR = /cygdrive/e/DEC/tools/exe
else ifeq ($(OPSYS),Linux)
# unix: UNIX comms model under LINUX, use PARMRK serial mode
OPTIONS = -DLINUX -DUSE_PARMRK
LFLAGS = -lpthread -lrt
BINDIR = /usr/local/bin
else # unknown environment
OPTIONS =
LFLAGS = -lpthread -lrt
BINDIR = /usr/local/bin
endif

# default program name, redefine PROG=xxx on command line if wanted
PROG = tu58em

# compiler flags and libraries
CC = gcc
CFLAGS = -I. -O3 -Wall -c $(OPTIONS)

$(PROG) : main.o tu58drive.o file.o serial.o
	$(CC) -o $@ main.o tu58drive.o file.o serial.o $(LFLAGS)

config :
	@echo "   OPSYS = \"$(OPSYS)\""
	@echo "    PROG = \"$(PROG)\""
	@echo "  BINDIR = \"$(BINDIR)\""
	@echo "      CC = \"$(CC)\""
	@echo "  CFLAGS = \"$(CFLAGS)\""
	@echo "  LFLAGS = \"$(LFLAGS)\""

clean :
	-rm -f *.o
	-chmod a-x,ug+w,o-w *.c *.h makefile
	-chmod a+rx $(PROG) $(PROG).exe
	-chown `whoami` *

purge : clean
	-rm -f $(PROG) $(PROG).exe

install : $(PROG)
	[ -d $(BINDIR) ] && cp $< $(BINDIR)

serial.o : serial.c common.h
	$(CC) $(CFLAGS) serial.c

main.o : main.c common.h
	$(CC) $(CFLAGS) main.c

tu58drive.o : tu58drive.c tu58.h common.h
	$(CC) $(CFLAGS) tu58drive.c

file.o : file.c common.h
	$(CC) $(CFLAGS) file.c

# the end
