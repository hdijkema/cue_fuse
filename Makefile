
all: bin/mp3cuefuse

bin/mp3cuefuse: elementals/libelementals.a
	(cd src; make)
	mkdir -p bin
	mv src/mp3cuefuse bin

elementals/libelementals.a: elementals
	(cd elementals;git pull)
	(cd elementals;make)

elementals:
	git clone git://github.com/hoesterholt/elementals.git

clean:
	rm -rf bin *~
	(cd src;make clean)
	(cd elementals; make clean)

distclean: clean
	rm -rf elementals
	

