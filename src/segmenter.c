#include "segmenter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <libmp3splt/mp3splt.h>
#include <id3tag.h>
#include "log.h"



/**********************************************************************/

static char *getExt(const char *filename) {
	int i=strlen(filename)-1;
	for(;i>=0 && filename[i]!='.';i--);
	if (i<0) { return strdup(""); }
	else { return strdup(&filename[i+1]); }
}

static void replace(char **s,const char *n) {
  free(*s);
  if (n==NULL) {
	  *s=strdup("");
  } else {
	*s=strdup(n);
  }
}

/**********************************************************************/

static void mp3splt_writer(const void *ptr, size_t size, size_t nmemb, void *cb_data) {
	FILE *f=(FILE *) cb_data;
	//log_debug4("Writing %d, %d, f=%p",size,nmemb,f);
	fwrite(ptr,size,nmemb,f);
}

static int mp3splt(segmenter_t *S) {
    char *ext=getExt(S->segment.filename);

    log_debug("mp3splt_split: entered");

	free(S->memory_block);
	S->size=-1;
	S->memory_block=NULL;
	FILE *f=open_memstream((char **) &S->memory_block,&S->size);

	log_debug2("memstream f=%p",f);

    int begin_offset_in_hs=S->segment.begin_offset_in_ms/10;
    int end_offset_in_hs=-1;
    if (S->segment.end_offset_in_ms>=0)  {
        end_offset_in_hs=S->segment.end_offset_in_ms/10;
    }

    log_debug("creating state");

    splt_state *state= mp3splt_new_state(NULL);
    mp3splt_find_plugins(state);
    //mp3splt_set_path_of_split(state,"/tmp");
    mp3splt_set_filename_to_split(state,S->segment.filename);
    //mp3splt_set_int_option(state, SPLT_OPT_OUTPUT_FILENAMES, SPLT_OUTPUT_CUSTOM);
    log_debug("setting pretend mode");
    mp3splt_set_int_option(state, SPLT_OPT_TAGS, SPLT_TAGS_ORIGINAL_FILE);
    //mp3splt_set_int_option(state,SPLT_OPT_TAGS, SPLT_CURRENT_TAGS);
    mp3splt_set_int_option(state, SPLT_OPT_PRETEND_TO_SPLIT, SPLT_TRUE);
    log_debug("pretend mode set");
    mp3splt_set_pretend_to_split_write_function(state,mp3splt_writer,(void *) f);
    mp3splt_read_original_tags(state);

	log_debug("created state");

	splt_point *point = mp3splt_point_new(begin_offset_in_hs, NULL);
	mp3splt_point_set_type(point,SPLT_SPLITPOINT);
	mp3splt_append_splitpoint(state,point);

	splt_point *skip = mp3splt_point_new(end_offset_in_hs, NULL);
	mp3splt_point_set_type(skip,SPLT_SKIPPOINT);
	mp3splt_append_splitpoint(state,skip);

	log_debug("added splitpoints");

    {
		splt_tags *tags=mp3splt_tags_new(NULL);

        char *title=S->segment.title;
        char *artist=S->segment.artist;
        char *album=S->segment.album;
        char *performer=S->segment.album_artist;
        char year[20];
        sprintf(year,"%d",S->segment.year);
        char *comment=S->segment.comment;
        char *genre=S->segment.genre;
        char track[20];
        sprintf(track,"%d",S->segment.track);
        //int track=S->segment.track;

        log_debug5("title=%s,artist=%s,album=%s,genre=%s",title,artist,album,genre);

        mp3splt_tags_set(tags, SPLT_TAGS_ORIGINAL, "true", NULL);
        mp3splt_tags_set(tags,
						 SPLT_TAGS_TITLE,title,
						 SPLT_TAGS_ARTIST,artist,
						 SPLT_TAGS_ALBUM,album,
						 SPLT_TAGS_PERFORMER,performer,
						 SPLT_TAGS_YEAR,year,
						 SPLT_TAGS_COMMENT,comment,
						 SPLT_TAGS_GENRE,genre,
						 SPLT_TAGS_TRACK,track,
						 NULL);

		mp3splt_append_tags(state,tags);

    }

    int error = SPLT_OK;
    error=mp3splt_split(state);
    int err;
    mp3splt_free_state(state);

    log_debug2("mp3splt_split: result=%d",error);

	fclose(f);

    if (error==SPLT_OK_SPLIT || error==SPLT_OK_SPLIT_EOF) {
		free(ext);
		return SEGMENTER_OK;
    } else {
		free(ext);
		return SEGMENTER_ERR_CREATE;
    }
}


/**********************************************************************/
static int split_mp3(segmenter_t *S) {
  return mp3splt(S);
}

static int split_ogg(segmenter_t *S) {
  return mp3splt(S);
}

/**********************************************************************/

segmenter_t *  segmenter_new() {
	segmenter_t *s=(segmenter_t *) malloc(sizeof(segmenter_t));
	s->memory_block=NULL;
	s->size=-1;
	s->last_result=SEGMENTER_NONE;
	s->segment.filename=strdup("");
	s->segment.artist=strdup("");
	s->segment.album=strdup("");
	s->segment.album_artist=strdup("");
	s->segment.title=strdup("");
	s->segment.composer=strdup("");
	s->segment.comment=strdup("");
	s->segment.genre=strdup("");
	s->segment.track=-1;
	s->stream=NULL;
	return s;
}

int segmenter_last_result(segmenter_t *S) {
	return S->last_result;
}

int segmenter_can_segment(segmenter_t *S,const char *filename) {
	char *ext=getExt(filename);
	int ok=(strcasecmp(ext,"mp3")==0) || (strcasecmp(ext,"ogg")==0);
	free(ext);
	return ok;
}

void segmenter_destroy(segmenter_t *S) {
	free(S->memory_block);
	free(S->segment.title);
	free(S->segment.artist);
	free(S->segment.album);
	free(S->segment.album_artist);
	free(S->segment.comment);
	free(S->segment.composer);
	free(S->segment.filename);
	free(S->segment.genre);
	if (S->stream!=NULL) { fclose(S->stream); }
	free(S);
}

int segmenter_create(segmenter_t *S) {
  char *filename=S->segment.filename;
  char *ext=getExt(filename);
  if (ext==NULL) {
	  S->last_result=SEGMENTER_ERR_FILETYPE;
	  return S->last_result;
  } else if (strcasecmp(ext,"mp3")==0) {
	  free(ext);
	  return split_mp3(S);
  } else if (strcasecmp(ext,"ogg")==0) {
	  free(ext);
	  return split_ogg(S);
  } else {
	  free(ext);
	  S->last_result=SEGMENTER_ERR_FILETYPE;
	  return S->last_result;
  }
}

int segmenter_open(segmenter_t *S) {
	if (S->memory_block==NULL) {
		S->last_result=SEGMENTER_ERR_NOSEGMENT;
		S->stream=NULL;
		S->last_result;
	} else {
		S->stream=fmemopen (S->memory_block, S->size, "rb");
		if (S->stream!=NULL) {
			S->last_result=SEGMENTER_OK;
		} else {
			S->last_result=SEGMENTER_ERR_FILEOPEN;
		}
		return S->last_result;
	}
}

int segmenter_close(segmenter_t *S) {
	if (S->stream!=NULL) {
		fclose(S->stream);
		S->stream=NULL;
		S->last_result=SEGMENTER_OK;
	} else {
		S->last_result=SEGMENTER_ERR_NOSTREAM;
	}
	return S->last_result;
}

FILE *segmenter_stream(segmenter_t *S) {
	return S->stream;
}

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
                        ) {
	segment_t *seg=&S->segment;
	replace(&seg->filename,filename);
	replace(&seg->title,title);
	replace(&seg->artist,artist);
	replace(&seg->album,album);
	replace(&seg->album_artist,album_artist);
	replace(&seg->composer,composer);
	replace(&seg->comment,comment);
	replace(&seg->genre,genre);
	seg->year=year;
	seg->begin_offset_in_ms=begin_offset_in_ms;
	seg->end_offset_in_ms=end_offset_in_ms;
	seg->track=track;
}

size_t segmenter_size(segmenter_t *S) {
	return S->size;
}

int segmenter_retcode(segmenter_t *S) {
    return S->last_result;
}

