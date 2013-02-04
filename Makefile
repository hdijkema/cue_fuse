
FUSE_CFLAGS=$(shell pkg-config --cflags fuse)
FUSE_LDFLAGS=$(shell pkg-config --libs fuse)

MP3SPLT_CFLAGS=-I/usr/local/include
#MP3SPLT_LDFLAGS=-L/usr/local/lib -lmp3splt
MP3SPLT_LDFLAGS=-L/usr/local/lib -lmp3splt

CC=cc
CFLAGS=-c $(FUSE_CFLAGS) $(MP3SPLT_CFLAGS)
LDFLAGS=$(FUSE_LDFLAGS) $(MP3SPLT_LDFLAGS)

all: mp3cuefuse

mp3cuefuse: mp3cuefuse.o btree.o cue.o segmenter.o list.o
	$(CC) -o mp3cuefuse mp3cuefuse.o btree.o cue.o segmenter.o list.o $(LDFLAGS)

mp3cuefuse.o: mp3cuefuse.c
	$(CC) $(CFLAGS) mp3cuefuse.c

btree.o : btree.c
	$(CC) $(CFLAGS) btree.c

cue.o : cue.c
	$(CC) $(CFLAGS) cue.c

log.o : log.c
	$(CC) $(CFLAGS) log.c
	
segmenter.o : segmenter.c
	$(CC) $(CFLAGS) segmenter.c

list.o : list.c
	$(CC) $(CFLAGS) list.c

test: list.o test_list.o test_seg.o 
	$(CC) -o test_list list.o test_list.o
	./test_list
	
test_list.o : test_list.c
	$(CC) -c test_list.c

test_seg: test_seg.o segmenter.o 
	$(CC) -o test_seg test_seg.o segmenter.o $(LDFLAGS)

test_seg.o : test_seg.c
	$(CC) $(CFLAGS) test_seg.c

clean:
	rm -f *.o *~ mp3cuefuse test_list test_seg
