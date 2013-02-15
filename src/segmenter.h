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
#ifndef __SEGMENTER__HOD
#define __SEGMENTER__HOD

#include <stdio.h>
#include <elementals/memblock.h>

typedef struct {
  int track, year, begin_offset_in_ms, end_offset_in_ms;
  char *title, *artist, *album, *album_artist, *composer, *comment, *genre;
  char *filename;
} segment_t;

typedef struct {
  memblock_t *blk;
  int last_result;
  segment_t segment;
  int stream;
} segmenter_t;

#define SEGMENTER_OK        0
#define SEGMENTER_NONE       10

#define SEGMENTER_ERROR      -1
#define SEGMENTER_ERR_CREATE    -10
#define SEGMENTER_ERR_FILETYPE  -20
#define SEGMENTER_ERR_FILEOPEN  -30
#define SEGMENTER_ERR_NOSEGMENT -31
#define SEGMENTER_ERR_NOMEM     -40
#define SEGMENTER_ERR_NOSTREAM  -29

segmenter_t *segmenter_new();
void segmenter_destroy(segmenter_t * S);
int segmenter_last_result(segmenter_t * S);
int segmenter_can_segment(segmenter_t * S, const char *filename);
void segmenter_prepare(segmenter_t * S,
           const char *filename,
           int track,
           const char *title,
           const char *artist,
           const char *album,
           const char *album_artist,
           const char *composer,
           const char *genre, int year, const char *comment, int begin_offset_in_ms, int end_offset_in_ms);

int segmenter_create(segmenter_t * S);
int segmenter_open(segmenter_t * S);
size_t segmenter_size(segmenter_t * S);
int segmenter_close(segmenter_t * S);
int segmenter_stream(segmenter_t * S);
int segmenter_retcode(segmenter_t * S);
int segmenter_read(segmenter_t * S, void *mem, size_t size);
void segmenter_seek(segmenter_t * S, off_t pos);
const char *segmenter_title(segmenter_t *S);

#endif
