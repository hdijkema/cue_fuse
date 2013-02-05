#include "segmenter.h"
#include "log.h"
#include <stdio.h>

FILE * log_handle() {
	return stderr;
}

int main() {
	segmenter_t * S=segmenter_new();
	segmenter_prepare(S,"/tmp/10cc.mp3",9,"9","a9","dreadlock","10cc","-","Pop",1970,"",60*1000,120*1000);
	segmenter_create(S);
	FILE *fout=fopen("/tmp/10cca.mp3","wb");
	log_debug2("opening %d",segmenter_open(S));
	FILE *fin=segmenter_stream(S);
	log_debug3("fin=%p, fout=%p",fin,fout);
    int s;
    char buf[1024];
    while ((s=fread(buf,1,1024,fin))>0) {
       fwrite(buf,s,1,fout);
    }
    segmenter_close(S);
	fclose(fout);
	segmenter_destroy(S);
	return 0;
}
