#
# tu58em emulator makefile
#

ifeq ($(comm),mac)
# UNIX comms model, but on a Mac
PROG = tu58em
COMM = -UWINCOMM -DMACOSX
else ifeq ($(comm),win)
# WINDOWS comms model
PROG = tu58ew
COMM = -DWINCOMM
else # ifeq ($(comm),unix)
# UNIX comms model
PROG = tu58em
COMM = -UWINCOMM
endif

BIN = ../../../../../tools/exe

CC = gcc
CFLAGS = -I. -O3 -Wall -c $(COMM)
ifeq ($(comm),mac)
LFLAGS = -lpthread
else
LFLAGS = -lpthread -lrt
endif

$(PROG) : main.o tu58drive.o file.o serial.o
	$(CC) -o $@ main.o tu58drive.o file.o serial.o $(LFLAGS)

all :
	make --always comm=win
	make clean
	make --always comm=unix
	make clean

mac :
	make --always comm=mac
	make --clean

installall :
	make --always comm=win install
	make clean
	make --always comm=unix install
	make clean

clean :
	-rm -f *.o
	-chmod a-x,ug+w,o-w *.c *.h makefile
	-chmod a+rx $(PROG) $(PROG).exe
	-chown `whoami` *

purge : clean
	-rm -f $(PROG) $(PROG).exe

install : $(PROG)
	cp $< $(BIN)

serial.o : serial.c common.h
	$(CC) $(CFLAGS) serial.c

main.o : main.c common.h
	$(CC) $(CFLAGS) main.c

tu58drive.o : tu58drive.c tu58.h common.h
	$(CC) $(CFLAGS) tu58drive.c

file.o : file.c common.h
	$(CC) $(CFLAGS) file.c

# the end
