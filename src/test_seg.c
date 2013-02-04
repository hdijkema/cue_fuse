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
	segmenter_destroy(S);
	return 0;
}
/*
void            segmenter_destroy(segmenter_t *S);
int             segmenter_last_result(segmenter_t *S);
int             segmenter_can_segment(segmenter_t *S,
									  const char *filename);
void segmenter_prepare(segmenter_t *S,
                        const char *filename,
						int track, 
                        const char *title,
                        const char *artist,
                        const char *album,
                        const char *album_artist,
                        const char *composer,
                        const char *genre,
                        int year,
                        const char *comment,
                        int begin_offset_in_ms,
                        int end_offset_in_ms
                        );
                        
int             segmenter_create(segmenter_t *S);
int             segmenter_open(segmenter_t *S);
size_t           segmenter_size(segmenter_t *S);
int             segmenter_close(segmenter_t *S);
FILE *          segmenter_stream(segmenter_t *S);
*/
