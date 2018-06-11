include Makefile.configure

all: libdiff.a main

main: diff.o compats.o main.o
	$(CC) -o $@ diff.o main.o compats.o

libdiff.a: diff.o compats.o
	$(AR) rs $@ diff.o compats.o

clean:
	rm -f libdiff.a diff.o compats.o main.o main

distclean: clean
	rm -f config.log config.h Makefile.configure

diff.o main.o: diff.h config.h
