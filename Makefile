
PWD=$(shell pwd)
PREFIX=/usr/local
INSTALL=/tmp/mp3cuefuse
MAJOR=0
MINOR=23
VERSION=${MAJOR}.${MINOR}

all: bin/mp3cuefuse 

bin/mp3cuefuse: mp3cuefuse mp3splt_sup/lib/libpm3splt.so version
	(cd src; make)
	mkdir -p bin
	mv src/mp3cuefuse_bin bin
	cp mp3cuefuse bin

version:
	@echo "#ifndef __CUEFUSE_VERSION_H" >version.h
	@echo "#define __CUEFUSE_VERSION_H" >>version.h
	@echo "#define MP3CUEFUSE_VERSION_MAJOR ${MAJOR}" >>version.h
	@echo "#define MP3CUEFUSE_VERSION_MINOR ${MINOR}" >>version.h
	@echo "#endif" >>version.h

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
	rm -f version.h

distclean: clean
	rm -rf elementals 

osx: version
	(cd src; make)
	mkdir -p ${INSTALL}
	rm -rf ${INSTALL}/Mp3CueFuse.app
	mkdir ${INSTALL}/Mp3CueFuse.app
	(cd ~/software/cf; tar cf - .) | (cd ${INSTALL}/Mp3CueFuse.app; tar xf - )
	cp src/mp3cuefuse_bin ${INSTALL}/Mp3CueFuse.app/bin
	cp mp3cuefuse_osx ${INSTALL}/Mp3CueFuse.app/bin
	chmod 755 ${INSTALL}/Mp3CueFuse.app/bin/mp3cuefuse_osx
	tar cf - Contents | (cd ${INSTALL}/Mp3CueFuse.app;tar xf -)
	chmod 755 ${INSTALL}/Mp3CueFuse.app/Contents/MacOs/*
	chmod 755 ${INSTALL}/Mp3CueFuse.app/Contents/Mp3CueFuseSilent.app/Contents/MacOs/*

dmg: osx
	rm -f ${INSTALL}/Applications
	ln -s /Applications ${INSTALL}/Applications
	tools/create-dmg --window-size 400 200 --icon-size 96 --volname "Mp3CueFuse-${VERSION}" --icon "Mp3CueFuse.app" 50 10 --icon "Applications" 250 10 --vol-icns Contents/Resources/Mp3CueFuse.icns ~/Desktop/Mp3CueFuse-${VERSION}.dmg ${INSTALL}

websf:
	(cd Web;scp *.css *.html *.png *.jpg hoesterholt@web.sourceforge.net:/home/project-web/cuefuse/htdocs)
