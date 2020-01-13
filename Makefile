CC=gcc
CFLAGS+=-std=c99 -Wall -I. -O3 -lrt

DEPS = xalloc.h hmap.h duration.h
OBJ1 = hmap.o hmap_test.o
OBJ2 = hmap.o hmap_example.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)
all: hmap_test hmap_example

hmap_test: $(OBJ1)
	$(CC) -o $@ $^ $(CFLAGS)
hmap_example: $(OBJ2)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -rf *.o *~ core hmap_test hmap_example
