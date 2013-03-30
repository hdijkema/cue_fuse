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

#include "cue.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <elementals.h>

#define T(a)  (a==NULL) ? "" : a

#define mystrdup(s) (s == NULL) ? NULL : mc_strdup(s)

static char* readline(FILE* f)
{
  char* line = mystrdup("");
  char buf[10240];
  int readsome = 0;

  while (fgets(buf, 10240, f) != NULL) {
    readsome = 1;
    {
      int i, ret = 0;
      for (i = 0; buf[i] != '\0' && buf[i] != '\n'; i++) ;
      if (buf[i] == '\n') {
        buf[i] = '\0';
        ret = 1;
      }
      line = (char* )mc_realloc(line, strlen(line) + 1 + i + 1);
      strcat(line, buf);
      if (ret) {
        return line;
      }
    }
  }

  if (readsome) {
    return line;
  } else {
    mc_free(line);
    return NULL;
  }
}

static char* mytrim(const char* line)
{
  return hre_trim_copy(line);
  /*
  int i, j;
  for (i = 0; line[i] != '\0' && isspace(line[i]); i++) ;
  char* k = mystrdup(&line[i]);
  for (j = strlen(k) - 1; j >= 0 && isspace(k[j]); j--) ;
  if (j >= 0) {
    k[j + 1] = '\0';
  }
  return k;*/
}

#define trim(a) (char* ) mc_take_over(mytrim(a))

static void mytrim_replace(char** line)
{
  char* rl = trim(*line);
  mc_free(*line);
  *line = rl;
}

#ifdef MEMCHECK
#define trim_replace(q) mytrim_replace(q),mc_take_over(*q)
#else
#define trim_replace(q) mytrim_replace(q)
#endif

static int eq(const char* s, const char* e)
{
  char* r = trim(s);
  
  int lr = strlen(r);
  int le = strlen(e);
  
  if (lr < le) {
    mc_free(r);
    return 0;
  }
  
  //log_debug6("%-15s %s, %d, %d %d", r, e, lr, le, strncasecmp(r, e, le));
  //log_debug3("%d, %d", r[0], e[0]);
  
  if (strncasecmp(r, e, le) == 0) {
    mc_free(r);
    return 1;
  } else {
    mc_free(r);
    return 0;
  }
}

static char* myunquote(const char* s, const char* e)
{
  char* r = trim(s);
  char* p = &r[strlen(e)];
  while (isspace(p[0]) && p[0] != '\0') {
    p++;
  }
  if (p[0] == '\0') {
    return mystrdup("");
  } else {
    if (p[0] == '"') {
      p += 1;
    }
    if (p[strlen(p) - 1] == '"') {
      p[strlen(p) - 1] = '\0';
    }
    char* k = mystrdup(p);
    trim_replace(&k);
    mc_free(r);
    return k;
  }
}

#define unquote(s,e)  (char* ) mc_take_over(myunquote(s,e))

static char* getFilePart(const char* s)
{
  int i;
  for (i = 0; s[i] != '"' && s[i] != '\0'; i++) ;
  char* fl = mc_strdup(&s[i]);
  for (i = strlen(fl) - 1; i >= 0 && fl[i] != '"'; i--) ;
  fl[i + 1] = '\0';
  return fl;
}

static cue_entry_t* cue_entry_new(cue_t* s)
{
  cue_entry_t* r = (cue_entry_t* ) mc_malloc(sizeof(cue_entry_t));
  if (r == NULL) {
    return NULL;
  }
  r->title = NULL;
  r->performer = NULL;
  r->year = NULL;
  r->composer = NULL;
  r->tracknr = -1;
  r->begin_offset_in_ms = -1;
  r->end_offset_in_ms = -1;
  r->sheet = (void* )s;
  r->vfile = NULL;
  r->audio_file = NULL;
  return r;
}

static void addEntry(cue_t* r, cue_entry_t* entry)
{
  r->count += 1;
  r->entries = (cue_entry_t** ) mc_realloc(r->entries, sizeof(cue_entry_t* ) * r->count);
  r->entries[r->count - 1] = entry;
}

static int calculateOffset(const char* in)
{
  int i;
  for (i = 0; isspace(in[i]); i++) ;
  if (in[i] >= '0' && in[i] <= '9') {
    for (; in[i] >= '0' && in[i] <= '9'; i++) ;
    if (in[i] != '\0') {
      for (; isspace(in[i]); i++) ;
      if (in[i] >= '0' && in[i] <= '9') {
        const char* min = &in[i];
        for (; in[i] != ':' && in[i] != '\0'; i++) ;
        if (in[i] == '\0') {
          return -1;
        }
        i += 1;
        const char* sec = &in[i];
        for (; in[i] != ':' && in[i] != '\0'; i++) ;
        if (in[i] == '\0') {
          return -1;
        }
        i += 1;
        const char* hs = &in[i];

        {
          int m = atoi(min);
          int s = atoi(sec);
          int ms = atoi(hs) * 10;
          return m * 60 * 1000 + s * 1000 + ms;
        }

      } else {
        return -1;
      }
    } else {
      return -1;
    }
  } else {
    return -1;
  }
}

static char* getExt(const char* filename)
{
  int i = strlen(filename) - 1;
  for (; i >= 0 && filename[i] != '.'; i--) ;
  if (i < 0) {
    return mystrdup("");
  } else {
    return mystrdup(&filename[i + 1]);
  }
}

/**************************************************************/

cue_t* cue_new(const char* file)
{
  cue_t* r = (cue_t* ) mc_malloc(sizeof(cue_t));

  r->album_title = NULL;
  r->album_performer = NULL;
  r->album_composer = NULL;
  r->image_file = NULL;
  r->genre = NULL;
  r->cuefile = mystrdup(file);
  r->count = 0;
  r->entries = NULL;
  r->_errno = 0;

  FILE*f = fopen(file, "rt");
  time_t _audio_mtime=0;
  char* audio_file = NULL;

  if (f == NULL) {
    r->_errno = ENOFILECUE;
  } else {
    char* line;
    //char* image = NULL;
    char* year = NULL;
    cue_entry_t* entry = NULL;
    int in_tracks = 0;
    
    while ((line = readline(f)) != NULL) {
      trim_replace(&line);
      if (strcmp(line, "") != 0) {
        if (!in_tracks) {
          if (eq(line, "performer")) {
            mc_free(r->album_performer);
            r->album_performer = unquote(line, "performer");
          } else if (eq(line, "title")) {
            mc_free(r->album_title);
            r->album_title = unquote(line, "title");
          } else if (eq(line, "file")) {
            mc_free(audio_file);
            char* fl = getFilePart(line);
            char* af = unquote(fl, "");
            if (strlen(af) > 0) {
              if (af[0] == '/') {
                audio_file = af;
              } else {
                char* cf = mc_strdup(r->cuefile);
                int ii;
                for (ii = strlen(cf) - 1; ii >= 0 && cf[ii] != '/'; ii--) ;
                if (ii >= 0) {
                  cf[ii] = '\0';
                  char* aaf = (char* )mc_malloc(strlen(cf) + strlen(af) + strlen("/") + 1);
                  sprintf(aaf, "%s/%s", cf, af);
                  audio_file = aaf;
                  mc_free(cf);
                  mc_free(af);
                } else {
                  audio_file = af;
                }
              }
            } else {
              audio_file = af;
            }
            // We have a full path audio file now.
            // get the mtime.
            {
              struct stat st;
              stat(audio_file,&st);
              _audio_mtime=st.st_mtime;
            }

            mc_free(fl);
          } else if (eq(line, "rem")) {
            if (eq(&line[3], "date")) {
              mc_free(year);
              year = unquote(&line[3], "date");
            } else if (eq(&line[3], "year")) {
              mc_free(year);
              year = unquote(&line[3], "year");
            } else if (eq(&line[3], "image")) {
              mc_free(r->image_file);
              r->image_file = unquote(&line[3], "image");
            } else if (eq(&line[3], "composer")) {
              mc_free(r->album_composer);
              r->album_performer = unquote(&line[3], "composer");
            } else if (eq(&line[3], "genre")) {
              mc_free(r->genre);
              r->genre = unquote(&line[3], "genre");
            }
          } else if (eq(line, "track")) {
            in_tracks = 1;
          } else {
            log_debug2("Skipping line '%s'", line);
          }
        }

        if (in_tracks) {
          if (eq(line, "track")) {
            if (entry != NULL) {
              addEntry(r, entry);
            }
            entry = cue_entry_new(r);
            entry->audio_mtime=_audio_mtime;
            entry->audio_file = mystrdup(audio_file);
            entry->year = mystrdup(year);
            entry->performer = mystrdup(r->album_performer);
            entry->composer = mystrdup(r->album_composer);
            entry->piece = NULL;
          } else if (eq(line, "title")) {
            mc_free(entry->title);
            entry->title = unquote(line, "title");
          } else if (eq(line, "performer")) {
            mc_free(entry->performer);
            entry->performer = unquote(line, "performer");
          } else if (eq(line, "index")) {
            char* index = unquote(line, "index");
            entry->begin_offset_in_ms = calculateOffset(index);
            mc_free(index);
          } else if (eq(line, "rem")) {
            if (eq(&line[3], "composer")) {
              mc_free(entry->composer);
              entry->composer = unquote(&line[3], "composer");
            } else if (eq(&line[3], "piece")) {
              mc_free(entry->piece);
              entry->piece = unquote(&line[3], "piece");
            } else if (eq(&line[3], "year")) {
              mc_free(year);
              year = unquote(&line[3], "year");
              mc_free(entry->year);
              entry->year = mystrdup(year);
            } else if (eq(&line[3], "date")) {
              mc_free(year);
              year = unquote(&line[3], "date");
              mc_free(entry->year);
              entry->year = mystrdup(year);
            }
          } else if (eq(line, "file")) {
            mc_free(audio_file);
            char* fl = getFilePart(line);
            char* af = unquote(fl, "");
            if (strlen(af) > 0) {
              if (af[0] == '/') {
                audio_file = af;
              } else {
                char* cf = mc_strdup(r->cuefile);
                int ii;
                for (ii = strlen(cf) - 1; ii >= 0 && cf[ii] != '/'; ii--) ;
                if (ii >= 0) {
                  cf[ii] = '\0';
                  char* aaf = (char* )mc_malloc(strlen(cf) + strlen(af) + strlen("/") + 1);
                  sprintf(aaf, "%s/%s", cf, af);
                  audio_file = aaf;
                  mc_free(cf);
                  mc_free(af);
                } else {
                  audio_file = af;
                }
              }
            } else {
              audio_file = af;
            }
            // We have a full path audio file now.
            // get the mtime.
            {
              struct stat st;
              stat(audio_file,&st);
              _audio_mtime=st.st_mtime;
            }

            mc_free(fl);
          }
        }
      }
      mc_free(line);
    }
    
    if (entry != NULL) {
      addEntry(r, entry);
    }
    
    mc_free(year);
    //mc_free(image);

    if (r->count > 0) {
      int i, N;
      for (i = 0, N = r->count-1; i < N; i++) {
        if (strcmp(r->entries[i+1]->audio_file, r->entries[i]->audio_file) == 0) {
          r->entries[i]->end_offset_in_ms = r->entries[i + 1]->begin_offset_in_ms;
        }
        r->entries[i]->tracknr = i + 1;
      }
      r->entries[i]->tracknr = i + 1;
    }
    
    fclose(f);
    
  }
  
  mc_free(audio_file);
  
  return r;
}

static void cue_destroy1(cue_t* c)
{
  mc_free(c->album_title);
  mc_free(c->album_performer);
  mc_free(c->album_composer);
  mc_free(c->genre);
  mc_free(c->cuefile);
  mc_free(c->entries);
  mc_free(c->image_file);
  mc_free(c);
}

void cue_destroy(cue_t* c)
{
  int i, N;
  for (i = 0, N = c->count; i < N; i++) {
    cue_entry_destroy(c->entries[0]);
  }
  if (N == 0) {
    cue_destroy1(c);
  }
}


int cue_valid(cue_t* c)
{
  return c->_errno == 0;
}

int cue__errno(cue_t* c)
{
  return c->_errno;
}

const char* cue_file(cue_t* cue)
{
  return T(cue->cuefile);
}

const char* cue_album_title(cue_t* cue)
{
  return T(cue->album_title);
}

const char* cue_image_file(cue_t* cue)
{
  return T(cue->image_file);
}

const char* cue_album_performer(cue_t* cue)
{
  return T(cue->album_performer);
}

const char* cue_album_composer(cue_t* cue)
{
  return T(cue->album_composer);
}

const char* cue_genre(cue_t* cue)
{
  return T(cue->genre);
}

const char* cue_audio_file(cue_t* cue)
{
  if (cue->count > 0) {
    return cue_entry_audio_file(cue_entry(cue, 0));
  } else {
    return T(NULL);
  }
}

int cue_count(cue_t* cue)
{
  return cue->count;
}

int cue_entries(cue_t* cue)
{
  return cue->count;
}

cue_entry_t* cue_entry(cue_t* cue, int index)
{
  if (index < 0) {
    return NULL;
  } else if (index >= cue->count) {
    return NULL;
  } else {
    return cue->entries[index];
  }
}

const char* cue_entry_title(cue_entry_t* ce)
{
  return T(ce->title);
}

const char* cue_entry_performer(cue_entry_t* ce)
{
  return T(ce->performer);
}

const char* cue_entry_composer(cue_entry_t* ce)
{
  return T(ce->composer);
}

const char* cue_entry_audio_file(cue_entry_t* ce)
{
  return T(ce->audio_file);
}

const char* cue_entry_piece(cue_entry_t* ce)
{
  return T(ce->piece);
}

const char* cue_entry_year(cue_entry_t* ce)
{
  return T(ce->year);
}

int cue_entry_tracknr(cue_entry_t* ce)
{
  return ce->tracknr;
}

int cue_entry_begin_offset_in_ms(cue_entry_t* ce)
{
  return ce->begin_offset_in_ms;
}

int cue_entry_end_offset_in_ms(cue_entry_t* ce)
{
  return ce->end_offset_in_ms;
}

cue_t* cue_entry_sheet(cue_entry_t* ce)
{
  return (cue_t* ) ce->sheet;
}

const char* cue_entry_vfile(cue_entry_t* ce)
{
  if (ce->vfile == NULL) {
    cue_t* c = (cue_t* ) ce->sheet;
    char* ext = getExt(cue_audio_file(c));
    char* name = (char* )mc_malloc(10 + strlen(cue_entry_title(ce)) + strlen(ext)+1);
    sprintf(name, "%02d - %s.%s", ce->tracknr, ce->title, ext);
    int i,N;
    for(i=0,N=strlen(name);i<N;i++) {
      if (name[i]=='/') { name[i]=' '; }
    }

    mc_free(ext);
    ce->vfile = name;
  }
  return ce->vfile;
}

char* cue_entry_alloc_id(cue_entry_t* ce)
{
  int l = strlen(cue_entry_vfile(ce)) + strlen(cue_entry_audio_file(ce)) + 1;
  char* s = (char* )mc_malloc(l);
  strcpy(s, cue_entry_vfile(ce));
  strcat(s, cue_entry_audio_file(ce));;
  return s;
}

void cue_entry_destroy(cue_entry_t* ce)
{
  cue_t* c = cue_entry_sheet(ce);
  int i, N;
  for (i = 0, N = cue_entries(c); i < N && cue_entry(c, i) != ce; i++) ;
  log_assert(i != N);

  cue_entry_t* e = ce;
  mc_free(e->audio_file);
  mc_free(e->title);
  mc_free(e->performer);
  mc_free(e->year);
  mc_free(e->composer);
  mc_free(e->piece);
  mc_free(e->vfile);
  mc_free(e);

  for (; i < N - 1; i++) {
    c->entries[i] = c->entries[i + 1];
  }
  c->count = N - 1;

  if (c->count <= 0) {
    cue_destroy1(c);
  }
}

int cue_entry_audio_changed(cue_entry_t* ce) {
  struct stat st;
  const char* af=cue_audio_file(cue_entry_sheet(ce));
  stat(af,&st);
  time_t at=ce->audio_mtime;
  if (at!=st.st_mtime) {
    return 1;
  } else {
    return 0;
  }
}

void cue_entry_audio_update_mtime(cue_entry_t* ce) {
  struct stat st;
  const char* af=cue_audio_file(cue_entry_sheet(ce));
  stat(af,&st);
  ce->audio_mtime=st.st_mtime;
}

