CC=gcc
CFLAGS=-Wall -g
BINS=libsegmalloc.o libsegmalloc.a

all: $(BINS)

libsegmalloc.o: segmalloc.c segmalloc.h
	$(CC) $(CFLAGS) -c segmalloc.c -o $@

libsegmalloc.a: segmalloc.c segmalloc.h
	ar rcs libsegmalloc.a libsegmalloc.o
	rm libsegmalloc.o

clean:
	rm *.a