# Project: rrdec
# Makefile created by Dev-C++ 4.9.8.0

CPP  = g++
CC   = gcc
WINDRES =
# windres.exe
RES  =
OBJ  = enc_list.o getopt.o getopt1.o rr.o rrenc.o rrfile.o rrlist.o rrmath.o $(RES)
LINKOBJ  = enc_list.o getopt.o getopt1.o rr.o rrenc.o rrfile.o rrlist.o $(RES)
LIBS =  -lrrmath.a
INCS =
CXXINCS =
BIN  = rrdec rrenc

#DEBUG = -DNDEBUG 
DEBUG = 
CXXFLAGS = $(CXXINCS) -Wall $(DEBUG) -D_CONSOLE -D_MBCS  -D__GCC__ -O3 
CFLAGS = $(INCS) -Wall $(DEBUG) -D_CONSOLE -D_MBCS -D__GCC__ -march=pentium4 -O3 -fexpensive-optimizations
#-march=athlon-fx

.PHONY: all all-before all-after clean clean-custom

all: all-before   rrdec rrenc all-after
#rrmath.a

clean: clean-custom
	rm -f $(OBJ) $(BIN) rrmath.a

rrenc:   rr.o rrenc.o rrfile.o rrlist.o  rrencode.o rrmath.o $(RES)
	$(CC)  rr.o rrenc.o rrfile.o rrlist.o rrdecode.o rrencode.o rrmath.o $(RES) -o "rrenc"
#rrdecode.o

#rrdec: getopt.o getopt1.o rr.o rrdec.o rrfile.o $(RES)
#	$(CC) getopt.o getopt1.o rr.o rrdec.o rrfile.o rrmath.a $(RES) -o "rrdec"
rrdec:  rr.o rrdec.o rrfile.o rrdecode.o rrmath.o $(RES)
	$(CC) rr.o rrdec.o rrfile.o rrdecode.o rrmath.o $(RES) -o "rrdec"
# rrencode.o 
rrmath.a: rrdecode.o rrencode.o rrmath.o $(RES)
	ar rcus rrmath.a rrdecode.o rrmath.o $(RES)
#	ar rcus ./Debug/librrmath.a ./Debug/rrdecode.o ./Debug/rrencode.o ./Debug/rrmath.o 
	ranlib rrmath.a

#getopt.o: getopt.c
#	$(CC) -c getopt.c -o getopt.o $(CFLAGS)

#getopt1.o: getopt1.c
#	$(CC) -c getopt1.c -o getopt1.o $(CFLAGS)

rr.o: rr.c
	$(CC) -c rr.c -o rr.o $(CFLAGS)

rrdec.o: rrdec.c
	$(CC) -c rrdec.c -o rrdec.o $(CFLAGS)

rrfile.o: rrfile.c
	$(CC) -c rrfile.c -o rrfile.o $(CFLAGS)

rrlist.o: rrlist.c
	$(CC) -c rrlist.c -o rrlist.o $(CFLAGS)

#enc_list.o: enc_list.c
#	$(CC) -c enc_list.c -o enc_list.o $(CFLAGS)

rrdecode.o: rrdecode.c
	$(CC) -c rrdecode.c -o rrdecode.o $(CFLAGS)

rrencode.o: rrencode.c
	$(CC) -c rrencode.c -o rrencode.o $(CFLAGS)

rrmath.o: rrmath.c
	$(CC) -c rrmath.c -o rrmath.o $(CFLAGS)
