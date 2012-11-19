CFLAGS=$(shell curl-config --cflags) -Wall -Werror $(EXTRA_CFLAGS)
LFLAGS=$(shell curl-config --libs) -largtable2 -lpthread
CC=gcc
AR=ar
ARFLAGS=-r
RANLIB=ranlib

default: wideload

%.o: %.c %.h
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

libb64.a: base64encode.o base64decode.o
	@$(AR) $(ARFLAGS) $@ $^
	@$(RANLIB) $@

wideload: list.o urlfile.o loader.o cli.o main.o libb64.a
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)


.PHONY: clean debug release

debug:
	make -f Makefile EXTRA_CFLAGS="-g -O0"

release: clean
	make -f Makefile EXTRA_CFLAGS="-O3"

clean:
	rm -rf *.o *.a *.dSYM
	rm -f wideload
