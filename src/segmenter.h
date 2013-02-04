#ifndef __SEGMENTER__HOD
#define __SEGMENTER__HOD

#include <stdio.h>

typedef struct {
	int track, 
		 year, 
		 begin_offset_in_ms,
		 end_offset_in_ms;
	char *title,
	      *artist,
		  *album,
		  *album_artist,
		  *composer,
		  *comment,
		  *genre;
	char *filename;
} segment_t;

typedef struct {
	void 	   *memory_block;
	size_t 		size;
	int   		last_result;
	FILE       *stream;
	segment_t  segment;
} segmenter_t;

#define SEGMENTER_OK 			  0
#define SEGMENTER_NONE  	 	 10

#define SEGMENTER_ERROR 		 -1
#define SEGMENTER_ERR_CREATE    -10
#define SEGMENTER_ERR_FILETYPE  -20
#define SEGMENTER_ERR_FILEOPEN  -30
#define SEGMENTER_ERR_NOSEGMENT -31
#define SEGMENTER_ERR_NOMEM     -40
#define SEGMENTER_ERR_NOSTREAM  -29

segmenter_t *  segmenter_new();
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



#endif
