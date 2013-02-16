
PWD=$(shell pwd)
PREFIX=/usr/local

all: bin/mp3cuefuse 

bin/mp3cuefuse: mp3cuefuse mp3splt_sup/lib/libpm3splt.so
	(cd src; make)
	mkdir -p bin
	mv src/mp3cuefuse_bin bin
	cp mp3cuefuse bin

mp3splt_sup/lib/libpm3splt.so: mp3splt_sup libmp3splt/src/.libs/libmp3splt.so
	(cd libmp3splt;make)
	(cd libmp3splt;make install)
	@echo ok
	
mp3splt_sup:
	mkdir -p mp3splt_sup

libmp3splt/src/.libs/libmp3splt.so: libmp3splt libmp3splt/config.status
	(cd libmp3splt;make)
	@echo ok
	
libmp3splt/config.status:
	(cd libmp3splt;./configure --prefix=${PWD}/mp3splt_sup)

	
libmp3splt:
	@if [ ! -d libmp3splt ]; then echo "Download libmp3splt >=0.8.2.1252 and put it in the libmp3splt subdir";exit 1; fi

install: bin/mp3cuefuse
	mkdir -p ${PREFIX}/bin
	cp bin/* ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/mp3cuefuse
	mkdir -p ${PREFIX}/bin/mp3splt_sup
	(cd mp3splt_sup;tar cf - .) | (cd ${PREFIX}/bin/mp3splt_sup;tar xvf -)

clean:
	rm -rf bin *~
	(cd src;make clean)
	if [ -d libmp3splt ]; then cd libmp3splt; make clean;rm -f config.status; fi
	rm -rf mp3splt_sup

distclean: clean
	rm -rf elementals 

osx: 
	(cd src; make)
	mkdir -p ~/tmp
	rm -rf ~/tmp/Mp3CueFuse.app
	mkdir ~/tmp/Mp3CueFuse.app
	(cd ~/software/cf; tar cf - .) | (cd ~/tmp/Mp3CueFuse.app; tar xf - )
	cp src/mp3cuefuse_bin ~/tmp/Mp3CueFuse.app/bin
	cp mp3cuefuse_osx ~/tmp/Mp3CueFuse.app/bin
	chmod 755 ~/tmp/Mp3CueFuse.app/bin/mp3cuefuse_osx
	tar cf - Contents | (cd ~/tmp/Mp3CueFuse.app;tar xf -)
	chmod 755 ~/tmp/Mp3CueFuse.app/Contents/MacOs/*
