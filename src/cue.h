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
#ifndef __CUE__HOD
#define __CUE__HOD

#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>

typedef struct {
  char *title;
  char *performer;
  char *year;
  char *composer;
  char *piece;
  char *audio_file;
  int tracknr;
  int begin_offset_in_ms;
  int end_offset_in_ms;
  void *sheet;
  char *vfile;
  time_t audio_mtime;
} cue_entry_t;

typedef struct {
  int _errno;
  char *album_title;
  char *album_performer;
  char *album_composer;
  char *genre;
  char *image_file;
  char *cuefile;
  int count;
  cue_entry_t **entries;
} cue_t;

#define ENOCUE    -1
#define EWRONGCUE   -2
#define ENOFILECUE  -3

cue_t *cue_new(const char *file);
void cue_destroy(cue_t *);

int cue_valid(cue_t *);
int cue_errno(cue_t *);
int cue_count(cue_t *);

const char *cue_file(cue_t * cue);
const char *cue_album_title(cue_t * cue);
const char *cue_album_performer(cue_t * cue);
const char *cue_album_composer(cue_t * cue);
const char *cue_audio_file(cue_t * cue);
const char *cue_genre(cue_t * cue);
const char *cue_image_file(cue_t* cue);

int cue_entries(cue_t * cue);
cue_entry_t *cue_entry(cue_t * cue, int index);

void cue_entry_destroy(cue_entry_t * ce); // destroys entry and removes it from cue
               // if it is the last one, destroys cue

const char *cue_entry_title(cue_entry_t * ce);
const char *cue_entry_performer(cue_entry_t * ce);
const char *cue_entry_composer(cue_entry_t * ce);
const char *cue_entry_piece(cue_entry_t * ce);
const char *cue_entry_year(cue_entry_t * ce);
const char *cue_entry_audio_file(cue_entry_t* ce);
int cue_entry_tracknr(cue_entry_t * ce);
int cue_entry_begin_offset_in_ms(cue_entry_t * ce);
int cue_entry_end_offset_in_ms(cue_entry_t * ce);
cue_t *cue_entry_sheet(cue_entry_t * ce);
const char *cue_entry_vfile(cue_entry_t * ce);
char *cue_entry_alloc_id(cue_entry_t * ce);

int cue_entry_audio_changed(cue_entry_t * ce);
void cue_entry_audio_update_mtime(cue_entry_t * ce);

#endif
