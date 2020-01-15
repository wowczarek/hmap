CC=gcc
CFLAGS+=-std=c99 -Wall -I. -L. -O3
LIBNAME = libhmap.a

LIBDEPS = duration.h xalloc.h hmap.h
LIBOBJ = hmap.o

OBJ1 = hmap.o hmap_test.o
OBJ2 = hmap.o hmap_example.o
OBJ1_DEPLIBS = -lrt
OBJ2_DEPLIBS =

%.o: %.c $(LIBDEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(LIBNAME) hmap_test hmap_example

$(LIBNAME): $(LIBOBJ)
	ar rc $@ $^
	ranlib $@

hmap_test: $(OBJ1)
	$(CC) -o $@ $^ $(OBJ1_DEPLIBS) $(CFLAGS)

hmap_example: $(OBJ2)
	$(CC) -o $@ $^ $(OBJ2_DEPLIBS) $(CFLAGS)

.PHONY: fast
fast:
	$(MAKE) -j8 $(LIBNAME)
	$(MAKE) -j8 all

.PHONY: clean
clean:
	rm -rf *.o *~ core hmap_test hmap_example $(LIBNAME)

remake: clean all

refast: clean fast

reprof: clean prof

prof: CFLAGS += -pg -Winline -Wpedantic
prof: all

debug: CFLAGS += -g -Winline -Wpedantic
debug: all

debugfast: CFLAGS += -g -Winline -Wpedantic
debugfast: fast

redebug: clean debug

redebugfast: CFLAGS += -g -Winline -Wpedantic
redebugfast: clean fast
