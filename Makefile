
PWD=$(shell pwd)

all: bin/mp3cuefuse

bin/mp3cuefuse: elementals/libelementals.a mp3splt/lib/libpm3splt.so
	(cd src; make)
	mkdir -p bin
	mv src/mp3cuefuse_bin bin
	cp mp3cuefuse bin

elementals/libelementals.a: elementals
	(cd elementals;git pull)
	(cd elementals;make)

elementals:
	git clone git://github.com/hoesterholt/elementals.git
	
mp3splt/lib/libpm3splt.so: mp3splt libmp3splt/src/.libs/libmp3splt.so
	(cd libmp3splt;make)
	(cd libmp3splt;make install)
	@echo ok
	
mp3splt:
	mkdir -p mp3splt

libmp3splt/src/.libs/libmp3splt.so: libmp3splt libmp3splt/config.status
	(cd libmp3splt;make)
	@echo ok
	
libmp3splt/config.status:
	(cd libmp3splt;./configure --prefix=${PWD}/mp3splt)

	
libmp3splt:
	@if [ ! -d libmp3splt ]; then echo "Download libmp3splt >=0.8.2.1252 and put it in the libmp3splt subdir";exit 1; fi

clean:
	rm -rf bin *~
	(cd src;make clean)
	(cd elementals; make clean)
	if [ -d libmp3splt ]; then cd libmp3splt; make clean;rm -f libmp3splt/config.status; fi
	rm -rf mp3splt

distclean: clean
	rm -rf elementals
	

	

