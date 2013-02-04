
all: bin/mp3cuefuse

bin/mp3cuefuse: elementals/libelementals.a
	(cd src; make)
	mkdir -p bin
	mv src/mp3cuefuse bin

elementals/libelementals.a: 
	(cd elementals;make)

clean:
	rm -rf bin *~
	(cd src;make clean)
	(cd elementals; make clean)

