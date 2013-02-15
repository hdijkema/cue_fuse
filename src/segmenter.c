/*
   This file is part of mp3cuefuse.
   Copyright 2013, Hans Oesterholt <debian@oesterholt.net>

   mp3cuefuse is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   mp3cuefuse is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with elementals.  If not, see <http://www.gnu.org/licenses/>.

   ********************************************************************
*/

#include "segmenter.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libmp3splt/mp3splt.h>
#include <id3tag.h>
#include <elementals/log.h>
#include <elementals/memcheck.h>
#include <elementals/memblock.h>

//#define SEGMENT_USING_FILE
#define GARD_WITH_MUTEX

#ifdef GARD_WITH_MUTEX
#include <pthread.h>
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/**********************************************************************/

static char *getExt(const char *filename)
{
  int i = strlen(filename) - 1;
  for (; i >= 0 && filename[i] != '.'; i--) ;
  if (i < 0) {
    return mc_strdup("");
  } else {
    return mc_strdup(&filename[i + 1]);
  }
}

static void replace(char **s, const char *n)
{
  mc_free(*s);
  if (n == NULL) {
    *s = mc_strdup("");
  } else {
    *s = mc_strdup(n);
  }
}

/**********************************************************************/

static void mp3splt_writer(const void *ptr, size_t size, size_t nmemb, void *cb_data)
{
  memblock_t *f = (memblock_t *) cb_data;
  memblock_write(f, ptr, size * nmemb);
}

/**********************************************************************/

static int mp3splt(segmenter_t * S)
{
  char *ext = getExt(S->segment.filename);

  //log_debug("mp3splt_split: entered");

  memblock_clear(S->blk);

  int begin_offset_in_hs = S->segment.begin_offset_in_ms / 10;
  int end_offset_in_hs = -1;
  if (S->segment.end_offset_in_ms >= 0) {
    end_offset_in_hs = S->segment.end_offset_in_ms / 10;
  }
  // Creating state
  splt_state *state = mp3splt_new_state(NULL);
  //log_debug("new state");
  mp3splt_find_plugins(state);
  //log_debug("plugins found");

  // Set split path and custom name
  mp3splt_set_path_of_split(state, "/tmp");
  //log_debug("split path set");
  mp3splt_set_int_option(state, SPLT_OPT_OUTPUT_FILENAMES, SPLT_OUTPUT_CUSTOM);
  //log_debug("custom split set");

  // Set filename to split and pretend mode, for memory based splitting
  mp3splt_set_filename_to_split(state, S->segment.filename);
  //log_debug("filename to split set");
  mp3splt_set_int_option(state, SPLT_OPT_PRETEND_TO_SPLIT, SPLT_TRUE);
  mp3splt_set_pretend_to_split_write_function(state, mp3splt_writer, (void *) S->blk);
  //log_debug("pretend split and write function set");

  // Create splitpoints
  splt_point *point = mp3splt_point_new(begin_offset_in_hs, NULL);
  mp3splt_point_set_type(point, SPLT_SPLITPOINT);
  mp3splt_append_splitpoint(state, point);

  splt_point *skip = mp3splt_point_new(end_offset_in_hs, NULL);
  mp3splt_point_set_type(skip, SPLT_SKIPPOINT);
  mp3splt_append_splitpoint(state, skip);
  //log_debug("split points set");

  // Append cuesheet tags and merge with existing
  {
    splt_tags *tags = mp3splt_tags_new(NULL);

    char *title = S->segment.title;
    char *artist = S->segment.artist;
    char *album = S->segment.album;
    char *performer = S->segment.album_artist;
    char year[20];
    sprintf(year, "%d", S->segment.year);
    char *comment = S->segment.comment;
    char *genre = S->segment.genre;
    char track[20];
    sprintf(track, "%d", S->segment.track);

    mp3splt_read_original_tags(state);
    //log_debug("original tags read");

    mp3splt_tags_set(tags, SPLT_TAGS_ORIGINAL, "true", NULL);
    //log_debug("SPLT_TAGS_ORIGINAL set");
    mp3splt_tags_set(tags,
         SPLT_TAGS_TITLE, title,
         SPLT_TAGS_ARTIST, artist,
         SPLT_TAGS_ALBUM, album,
         SPLT_TAGS_PERFORMER, performer,
         SPLT_TAGS_YEAR, year,
         SPLT_TAGS_COMMENT, comment, SPLT_TAGS_GENRE, genre, SPLT_TAGS_TRACK, track, NULL);
    //log_debug("tags set");
    mp3splt_append_tags(state, tags);
    //log_debug("tag appended");
  }

  // split the stuff
  int error = SPLT_OK;
  error = mp3splt_split(state);
  //log_debug("split done");
  mp3splt_free_state(state);
  //log_debug("state freeed");
  log_debug2("mp3splt_split: result=%d", error);

  if (error == SPLT_OK_SPLIT || error == SPLT_OK_SPLIT_EOF) {
    mc_free(ext);
    return SEGMENTER_OK;
  } else {
    mc_free(ext);
    return SEGMENTER_ERR_CREATE;
  }
}

/**********************************************************************/
static int split_mp3(segmenter_t * S)
{
  int result;
#ifdef GARD_WITH_MUTEX
  pthread_mutex_lock(&mutex);
#endif
  result = mp3splt(S);
#ifdef GARD_WITH_MUTEX
  pthread_mutex_unlock(&mutex);
#endif
  return result;
}

static int split_ogg(segmenter_t * S)
{
  int result;
#ifdef GARD_WITH_MUTEX
  pthread_mutex_lock(&mutex);
#endif
  result = mp3splt(S);
#ifdef GARD_WITH_MUTEX
  pthread_mutex_unlock(&mutex);
#endif
  return result;
}

/**********************************************************************/

segmenter_t *segmenter_new()
{
  segmenter_t *s = (segmenter_t *) mc_malloc(sizeof(segmenter_t));
  s->blk = memblock_new();
  s->stream = 0;
  s->last_result = SEGMENTER_NONE;
  s->segment.filename = mc_strdup("");
  s->segment.artist = mc_strdup("");
  s->segment.album = mc_strdup("");
  s->segment.album_artist = mc_strdup("");
  s->segment.title = mc_strdup("");
  s->segment.composer = mc_strdup("");
  s->segment.comment = mc_strdup("");
  s->segment.genre = mc_strdup("");
  s->segment.track = -1;
  return s;
}

int segmenter_last_result(segmenter_t * S)
{
  return S->last_result;
}

int segmenter_can_segment(segmenter_t * S, const char *filename)
{
  char *ext = getExt(filename);
  int ok = (strcasecmp(ext, "mp3") == 0) || (strcasecmp(ext, "ogg") == 0);
  mc_free(ext);
  return ok;
}

void segmenter_destroy(segmenter_t * S)
{
  memblock_destroy(S->blk);
  S->stream = 0;
  mc_free(S->segment.title);
  mc_free(S->segment.artist);
  mc_free(S->segment.album);
  mc_free(S->segment.album_artist);
  mc_free(S->segment.comment);
  mc_free(S->segment.composer);
  mc_free(S->segment.filename);
  mc_free(S->segment.genre);
  mc_free(S);
}

int segmenter_create(segmenter_t * S)
{
  // assert that this segment isn't opened.
  int reopen=0;
  if (segmenter_stream(S)) {
    reopen=1;
    segmenter_close(S);
  }

  char *filename = S->segment.filename;
  char *ext = getExt(filename);
  if (ext == NULL) {
    S->last_result = SEGMENTER_ERR_FILETYPE;
    return S->last_result;
  } else if (strcasecmp(ext, "mp3") == 0) {
    mc_free(ext);
    return split_mp3(S);
  } else if (strcasecmp(ext, "ogg") == 0) {
    mc_free(ext);
    return split_ogg(S);
  } else {
    mc_free(ext);
    S->last_result = SEGMENTER_ERR_FILETYPE;
    return S->last_result;
  }

  if (reopen) {
    segmenter_open(S);
  }
}

int segmenter_open(segmenter_t * S)
{
  if (memblock_size(S->blk) == 0) {
    S->last_result = SEGMENTER_ERR_NOSEGMENT;
    S->stream = 0;
    return S->last_result;
  } else {
    S->stream = 1;
    if (S->stream) {
      S->last_result = SEGMENTER_OK;
    } else {
      S->last_result = SEGMENTER_ERR_FILEOPEN;
    }
    return S->last_result;
  }
}

int segmenter_close(segmenter_t * S)
{
  if (S->stream) {
    S->stream = 0;
    S->last_result = SEGMENTER_OK;
  } else {
    S->last_result = SEGMENTER_ERR_NOSTREAM;
  }
  return S->last_result;
}

int segmenter_stream(segmenter_t * S)
{
  return S->stream;
}

void segmenter_prepare(segmenter_t * S,
           const char *filename,
           int track,
           const char *title,
           const char *artist,
           const char *album,
           const char *album_artist,
           const char *composer,
           const char *genre, int year, const char *comment, int begin_offset_in_ms, int end_offset_in_ms)
{
  segment_t *seg = &S->segment;
  replace(&seg->filename, filename);
  replace(&seg->title, title);
  replace(&seg->artist, artist);
  replace(&seg->album, album);
  replace(&seg->album_artist, album_artist);
  replace(&seg->composer, composer);
  replace(&seg->comment, comment);
  replace(&seg->genre, genre);
  seg->year = year;
  seg->begin_offset_in_ms = begin_offset_in_ms;
  seg->end_offset_in_ms = end_offset_in_ms;
  seg->track = track;
}

size_t segmenter_size(segmenter_t * S)
{
  return memblock_size(S->blk);
}

int segmenter_retcode(segmenter_t * S)
{
  return S->last_result;
}

int segmenter_read(segmenter_t *S, void *mem, size_t size) {
  return (int) memblock_read(S->blk, mem, size);
}

void segmenter_seek(segmenter_t *S, off_t pos) {
  memblock_seek(S->blk, pos);
}

const char *segmenter_title(segmenter_t *s) {
  return s->segment.title;
}
