
FUSE_CFLAGS=$(shell pkg-config --cflags fuse)
FUSE_LDFLAGS=$(shell pkg-config --libs fuse)

MP3SPLT_CFLAGS=-I../mp3splt/include
MP3SPLT_LDFLAGS=-L../mp3splt/lib -lmp3splt

CC=cc
CFLAGS=-c -I../elementals $(FUSE_CFLAGS) $(MP3SPLT_CFLAGS)
LDFLAGS=$(FUSE_LDFLAGS) $(MP3SPLT_LDFLAGS) -L../elementals -lelementals

all: mp3cuefuse ../elementals/libelementals.a
	mv mp3cuefuse mp3cuefuse_bin

mp3cuefuse: mp3cuefuse.o cue.o segmenter.o 
	$(CC) -o mp3cuefuse mp3cuefuse.o cue.o segmenter.o $(LDFLAGS)

mp3cuefuse.o: mp3cuefuse.c
	$(CC) $(CFLAGS) mp3cuefuse.c

cue.o : cue.c
	$(CC) $(CFLAGS) cue.c

segmenter.o : segmenter.c
	$(CC) $(CFLAGS) segmenter.c

test_seg: test_seg.o segmenter.o 
	$(CC) -o test_seg test_seg.o segmenter.o $(LDFLAGS)

test_seg.o : test_seg.c
	$(CC) $(CFLAGS) test_seg.c

../elementals/libelementals.a:
	(cd ../elementals;make)

clean:
	rm -f *.o *~ mp3cuefuse test_list test_seg
