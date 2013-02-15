
FUSE_CFLAGS=-I/usr/local/include/osxfuse -D_FILE_OFFSET_BITS=64
FUSE_LDFLAGS=/usr/local/lib -lfuse

MP3SPLT_CFLAGS=-I/Users/oesterholt/software/cf/include
MP3SPLT_LDFLAGS=-L/Users/oesterholt/software/cf/lib -lmp3splt
#MP3SPLT_CFLAGS=-I/usr/local/include
#MP3SPLT_LDFLAGS=-L/usr/local/lib -lmp3splt

CC=cc
#CFLAGS=-c -I../elementals -DUSE_MEMCHECK $(FUSE_CFLAGS) $(MP3SPLT_CFLAGS)
CFLAGS=-c -O2 -DUSE_MEMCHECK $(FUSE_CFLAGS) $(MP3SPLT_CFLAGS)
LDFLAGS=$(FUSE_LDFLAGS) $(MP3SPLT_LDFLAGS) -lelementals

all: mp3cuefuse 
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

minimal: minimal.c
	$(CC) $(CFLAGS) minimal.c
	$(CC) -o minimal minimal.o $(LDFLAGS)

clean:
	rm -f *.o *~ mp3cuefuse test_list test_seg minimal
