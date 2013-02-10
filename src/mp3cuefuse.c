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
#define FUSE_USE_VERSION  26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>

#include "cue.h"
#include "segmenter.h"

#include <elementals/hash.h>
#include <elementals/log.h>
#include <elementals/list.h>
#include <elementals/memcheck.h>
#include <elementals/os.h>

#define MKR(st,A) if (st.st_mode&A) { log_debug("modeadjust");st.st_mode-=A; }
#define MK_READONLY(st) MKR(st,S_IWUSR);MKR(st,S_IWGRP);MKR(st,S_IWOTH);

#define PMKR(st,A) if (st->st_mode&A) { log_debug("modeadjust");st->st_mode-=A; }
#define PMK_READONLY(st) PMKR(st,S_IWUSR);PMKR(st,S_IWGRP);PMKR(st,S_IWOTH);

#define false 0
#define true  1

/***********************************************************************/

static char *BASEDIR;
static int MAX_MEM_USAGE_IN_MB = 200;

/***********************************************************************/

int usage(char *p)
{
  fprintf(stderr, "%s [--memory|m maxMB] <cue directory> <mountpoint>\n", p);
  return 1;
}

/***********************************************************************/

static char *trim(const char *line)
{
  int i, j;
  for (i = 0; line[i] != '\0' && isspace(line[i]); i++) ;
  char *k = mc_strdup(&line[i]);
  for (j = strlen(k) - 1; j >= 0 && isspace(k[j]); j--) ;
  if (j >= 0) {
    k[j + 1] = '\0';
  }
  return k;
}

/***********************************************************************/

typedef struct {
  size_t size;
  time_t mtime;
} vfile_size_t;

hash_data_t vfile_size_copy(vfile_size_t *e) {
  vfile_size_t *fs=(vfile_size_t *) mc_malloc(sizeof(vfile_size_t));
  fs->size=e->size;
  fs->mtime=e->mtime;
}

void vfile_size_destroy(hash_data_t d) {
  log_debug("destroying size hash elem");
  vfile_size_t *fs=(vfile_size_t *) d;
  mc_free(fs);
}

DECLARE_HASH(vfilesize_hash, vfile_size_t);
IMPLEMENT_HASH(vfilesize_hash, vfile_size_t, vfile_size_copy, vfile_size_destroy);

static vfilesize_hash *SIZE_HASH = NULL;

void put_size(const char *vfile, size_t size, time_t mtime) {
  vfile_size_t *e = vfilesize_hash_get(SIZE_HASH, vfile);
  if (e!=NULL) {
    if (e->mtime != mtime) {
      vfile_size_t e = { size, mtime };
      vfilesize_hash_put(SIZE_HASH,vfile, &e);
    }
  } else {
    vfile_size_t e = { size, mtime };
    vfilesize_hash_put(SIZE_HASH,vfile, &e);
  }
}

int has_size(const char *vfile, time_t mtime) {
  vfile_size_t *e = vfilesize_hash_get(SIZE_HASH, vfile);
  if (e != NULL && e->mtime == mtime) {
    return 1;
  } else {
    return 0;
  }
}

size_t get_size(const char *vfile) {
  vfile_size_t *e = vfilesize_hash_get(SIZE_HASH, vfile);
  if (e != NULL) {
    return e->size;
  } else {
    return 0;
  }
}

#define VFILESIZE_FILE_TYPE     "type:mp3cuefuse-size-cache"
#define VFILESIZE_FILE_VERSION  "version:1"

void read_in_sizes(const char *from_file) {

  FILE *f = fopen(from_file, "rt");
  if (f==NULL) {
    return;
  }

  char *line = (char *) mc_malloc(10240*sizeof(char));
  if (fgets(line, 10240, f) != NULL) {
    char *ln = trim(line);
    if (strcmp(ln, VFILESIZE_FILE_TYPE) == 0) {
      if (fgets(line, 10240, f) != NULL) {
        char *ln1 = trim(line);
        if (strcmp(ln1, VFILESIZE_FILE_VERSION) == 0) {
          while (fgets(line, 10240, f) != NULL) {
            char *ln2 = trim(line);
            char *vfile = mc_strdup( ln2 );
            fgets(line, 10240, f);
            char *ln3 = trim( line );
            size_t size = (size_t) strtoul(ln3, NULL, 10);
            fgets(line, 10240, f);
            char *ln4 = trim (line);
            time_t mtime = (time_t) strtoul(ln4, NULL, 10);
            put_size( vfile, size, mtime );
            mc_free( vfile);
            mc_free( ln4 );
            mc_free( ln3 );
            mc_free( ln2 );
          }
        }
        mc_free( ln1 );
      }
    }
    mc_free( ln );
  }
  mc_free(line);
  fclose(f);
}

void write_sizes(const char * to_file) {
  FILE *f = fopen(to_file, "wt");
  fputs(VFILESIZE_FILE_TYPE /**/ "\n", f);
  fputs(VFILESIZE_FILE_VERSION /**/ "\n", f);
  hash_iter_t it;
  it = vfilesize_hash_iter(SIZE_HASH);
  while (!vfilesize_hash_iter_end(it)) {
    vfile_size_t *e = vfilesize_hash_get(SIZE_HASH, vfilesize_hash_iter_key(it) );
    if (e != NULL) {
      fprintf(f, "%s\n%lu\n%lu\n", vfilesize_hash_iter_key(it),
                  (unsigned long) e->size, (unsigned long) e->mtime );
    } else {
      log_debug("Unexpected!");
    }
    it = vfilesize_hash_iter_next(it);
  }
  fclose(f);
}

/***********************************************************************/

typedef struct {
  char *id;
  segmenter_t *segment;
} seg_entry_t;

list_data_t seg_entry_copy(seg_entry_t * e)
{
  return (list_data_t) e;
}

void seg_entry_destroy(list_data_t _e)
{
  log_debug("destroying segment");
  seg_entry_t *e = (seg_entry_t *) _e;
  mc_free(e->id);
  segmenter_destroy(e->segment);
  mc_free(e);
}

DECLARE_LIST(seglist, seg_entry_t);
IMPLEMENT_LIST(seglist, seg_entry_t, seg_entry_copy, seg_entry_destroy);

static list_t *SEGMENT_LIST = NULL;

void add_seg_entry(cue_entry_t * e, segmenter_t * s)
{
  log_debug("lock segment list");
  seglist_lock(SEGMENT_LIST);
  {
    log_debug("first look if we need to destroy a segment");
    seg_entry_t *se;
    se = seglist_start_iter(SEGMENT_LIST, LIST_FIRST);
    double count_mb = 0.0;
    while (se != NULL) {
      count_mb += segmenter_size(se->segment) / (1024.0 * 1024.0);
      se = seglist_next_iter(SEGMENT_LIST);
    }
    log_debug("drop last ones as long we're above our memory limit");
    int n = seglist_count(SEGMENT_LIST);
    int k = 0;
    while (((int)count_mb) > MAX_MEM_USAGE_IN_MB && k < 5) {
      se = seglist_start_iter(SEGMENT_LIST, LIST_LAST);
      if (se != NULL) {
        if (segmenter_stream(se->segment) == NULL) {
          count_mb -= segmenter_size(se->segment) / (1024.0 * 1024.0);
          seglist_drop_iter(SEGMENT_LIST);
          k = 0;
        } else {
          seglist_move_iter(SEGMENT_LIST, LIST_FIRST);
          k += 1;
        }
      } else {
        count_mb = 0;
      }
    }
    log_debug("add our segment on front");
    se = (seg_entry_t *) mc_malloc(sizeof(seg_entry_t));
    se->id = cue_entry_alloc_id(e);
    se->segment = s;
    seglist_start_iter(SEGMENT_LIST, LIST_FIRST);
    seglist_prepend_iter(SEGMENT_LIST, se);
  }
  log_debug("unlock segmentlist");
  seglist_unlock(SEGMENT_LIST);
}

segmenter_t *find_seg_entry(cue_entry_t * e)
{
  char *id = cue_entry_alloc_id(e);
  seglist_lock(SEGMENT_LIST);
  seg_entry_t *se = seglist_start_iter(SEGMENT_LIST, LIST_FIRST);
  while (se != NULL && strcmp(se->id, id) != 0) {
    se = seglist_next_iter(SEGMENT_LIST);
  }
  log_debug3("found segment %p for id %s", se, id);
  mc_free(id);
  seglist_unlock(SEGMENT_LIST);
  if (se == NULL) {
    return NULL;
  } else {
    return se->segment;
  }
}

/***********************************************************************/

pthread_mutex_t DATA_ENTRY_MONITOR=PTHREAD_MUTEX_INITIALIZER;

void enter_de_monitor() {
  pthread_mutex_lock(&DATA_ENTRY_MONITOR);
}

void leave_de_monitor() {
  pthread_mutex_unlock(&DATA_ENTRY_MONITOR);
}

#define DE_MONITOR(code) enter_de_monitor();code;leave_de_monitor()

typedef struct {
  cue_entry_t *entry;
  char *path;
  struct stat *st;
  int open_count;
} data_entry_t;

static data_entry_t *mydata_entry_new(const char *path, cue_entry_t * entry, struct stat *st)
{
  data_entry_t *e = (data_entry_t *) mc_malloc(sizeof(data_entry_t));
  e->path = mc_strdup(path);
  e->entry = entry;
  e->open_count = 0;

  if (st != NULL) {
    struct stat *stn = (struct stat *)mc_malloc(sizeof(struct stat));
    memcpy((void *)stn, (void *)st, sizeof(struct stat));
    e->st = stn;
  } else {
    e->st = NULL;
  }

  return e;
}

#define data_entry_new(p,e,s) (data_entry_t *) mc_take_over(mydata_entry_new(p,e,s))

static void data_entry_destroy(data_entry_t * e)
{
  log_debug2("Destroying cue entry %s", cue_entry_title(e->entry));
  cue_entry_destroy(e->entry);
  mc_free(e->path);
  mc_free(e->st);
  mc_free(e);
}

static hash_data_t data_copy(data_entry_t * e)
{
  return (hash_data_t) e;
}

static void data_destroy(hash_data_t d)
{
  log_debug("Destroying data entry");
  data_entry_t *e = (data_entry_t *) d;
  data_entry_destroy(e);
}

DECLARE_HASH(datahash, data_entry_t);
IMPLEMENT_HASH(datahash, data_entry_t, data_copy, data_destroy);

datahash *DATA = NULL;

/***********************************************************************/

char *mymake_path(const char *path)
{
  int l = strlen(path) + strlen(BASEDIR) + 1;
  char *np = (char *)mc_malloc(l);
  fprintf(stderr, "np=%p\n", np);
  if (np == NULL) {
    return np;
  } else {
    strcpy(np, BASEDIR);
    strcat(np, path);
    log_debug2("fullpath=%s", np);
    //fprintf(stderr,"fullpath=%s\n",np);
    return np;
  }
}

#define make_path(p) mc_take_over(mymake_path(p))

char *make_rel_path2(const char *path, const char *file)
{
  int pl = strlen(path);
  int l = strlen(path) + strlen("/") + strlen(file) + 1;
  char *fp = (char *)mc_malloc(l);
  if (fp == NULL) {
    return NULL;
  } else {
    strcpy(fp, path);
    if (pl > 0) {
      if (fp[pl - 1] != '/') {
        strcat(fp, "/");
      }
    }
    strcat(fp, file);
    return fp;
  }
}

char *make_path2(const char *path, const char *file)
{
  char *np = make_path(path);
  if (np == NULL) {
    return np;
  } else {
    char *r = make_rel_path2(np, file);
    mc_free(np);
    return r;
  }
}

static int isExt(const char *path, const char *ext)
{
  int l = strlen(ext);
  int pl = strlen(path);
  if (pl >= l) {
    if (strcasecmp(path + pl - l, ext) == 0) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

static int isCue(const char *path)
{
  return isExt(path, ".cue");
}

static int isImage(const char *path)
{
  return isExt(path, ".jpg") || isExt(path, ".jpeg")
      || isExt(path, ".png");
}

static char *stripExt(const char *_path, const char *ext)
{
  char *path = mc_strdup(_path);
  if (path == NULL) {
    return NULL;
  } else if (isExt(path, ext)) {
    int l = strlen(path) - strlen(ext);
    if (l > 0) {
      path[l] = '\0';
    }
  }
  return path;
}

static char *isCueFile(const char *full_path)
{
  char *fp = (char *)mc_malloc(strlen(full_path) + strlen(".cue") + 1);
  char *cues[] = { ".cue", ".Cue", ".cUe", ".cuE", ".CUe", ".CuE", ".cUE", ".CUE",
    NULL
  };
  int i;

  for (i = 0; cues[i] != NULL; i++) {
    sprintf(fp, "%s%s", full_path, cues[i]);
    FILE *f = fopen(fp, "r");
    if (f != NULL) {
      fclose(f);
      return fp;
    }
  }

  mc_free(fp);

  return NULL;
}

static char *getCueFileForTrack(const char *full_path_of_track, int with_ext) {
  char *fp = (char *)mc_malloc(strlen(full_path_of_track) + strlen(".cue") + 1);
  strcpy(fp, full_path_of_track);
  int i,N;
  for(N = strlen(fp), i = N-1; i >= 0 && fp[i] != '/'; --i);
  if (i<0) {
    log_error("Unexpected!");
    return fp;
  } else {
    fp[i] = '\0';
    if (with_ext) {
      strcat(fp, ".cue");
    }
    return fp;
  }
}

/***********************************************************************/

static segmenter_t *get_segment(cue_entry_t * e, int update)
{
  segmenter_t *se = find_seg_entry(e);
  if (se != NULL) {
    if (update) {
      segmenter_create(se);
    }
    return se;
  } else {
    cue_t *sheet = cue_entry_sheet(e);
    segmenter_t *s = segmenter_new();
    const char *fullpath = cue_audio_file(sheet);
    int year = atoi(cue_entry_year(e));
    segmenter_prepare(s,
          fullpath,
          cue_entry_tracknr(e),
          cue_entry_title(e),
          cue_entry_performer(e),
          cue_album_title(sheet),
          cue_album_performer(sheet),
          cue_entry_composer(e),
          cue_genre(sheet),
          year,
          cue_entry_piece(e), cue_entry_begin_offset_in_ms(e), cue_entry_end_offset_in_ms(e)
        );
    segmenter_create(s);
    add_seg_entry(e, s);
    return s;
  }
}

/***********************************************************************/

static list_data_t delist_copy(data_entry_t * e)
{
  return (list_data_t) e;
}

static void delist_destroy_entry(list_data_t e)
{
  data_entry_destroy((data_entry_t *) e);
}

DECLARE_LIST(delist, data_entry_t);
IMPLEMENT_LIST(delist, data_entry_t, delist_copy, delist_destroy_entry);

static cue_t *mp3cue_readcue_in_hash(const char *path, int update_data)
{
  char *fullpath = make_path(path);
  char *cuefile = isCueFile(fullpath);
  log_debug3("reading cuefile %s for %s", cuefile, fullpath);
  cue_t *cue = (cue_t *) mc_take_over(cue_new(cuefile));
  struct stat st;
  stat(cuefile, &st);
  MK_READONLY(st);
  if (cue != NULL && cue_valid(cue)) {
    int added = 0;
    int i, N;

    // check if cue already exists in hash. If not, create whole cue in hash and read in again.
    for (i = 0, N = cue_count(cue); i < N; i++) {
      cue_entry_t *entry = cue_entry(cue, i);
      char *p = make_path2(path, cue_entry_vfile(entry));

      log_debug2("p=%s", p);
      if (datahash_exists(DATA, p)) {
        // update entry only if requested
        if ( update_data ) {
          // only update the data in d, otherwise we won't be thread safe!
          // Never deallocate an entry in the hash!
          // We also know, that when the track title is changed, there will be a
          // new entry in the hash, so, we get some rubbish but don't care.
          // We update the file sizes and the cue_entry here.

          // So we need a monitor around the file operations that act on the hash
          // data.

          data_entry_t *dd = datahash_get(DATA, p);
          cue_entry_destroy(dd->entry);
          dd->entry=entry;
          dd->st[0]=st;

          // we don't need to update dd->path as dd->path and p must be equal
        }
      } else {
        data_entry_t *d = data_entry_new(p, entry, &st);
        datahash_put(DATA, p, d);
        added = 1;
      }

      mc_free(p);
    }

    if (added) {
      mc_free(cuefile);
      mc_free(fullpath);
      return mp3cue_readcue_in_hash(path, false);
    }
  }

  mc_free(cuefile);
  mc_free(fullpath);
  return cue;
}

static int mp3cue_readcue(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  log_debug2("enter with %s", path);
  cue_t *cue = mp3cue_readcue_in_hash(path, false);

  char *fullpath = make_path(path);
  char *cuefile = isCueFile(fullpath);

  struct stat st;
  stat(cuefile, &st);
  MK_READONLY(st);
  if (cue != NULL && cue_valid(cue)) {
    int i, N;
    for (i = 0, N = cue_count(cue); i < N; i++) {
      cue_entry_t *entry = cue_entry(cue, i);
      data_entry_t *d = datahash_get(DATA, fullpath);
      if (d != NULL) {
        filler(buf, cue_entry_vfile(entry), d->st, 0);
      } else {
        filler(buf, cue_entry_vfile(entry), &st, 0);
      }
    }
  }

  cue_destroy(cue);

  mc_free(cuefile);
  mc_free(fullpath);

  return 0;
}

/***********************************************************************
 File system operations. Here we use the DE_MONITOR. Nowhere else!
*/

static int mp3cue_getattr(const char *path, struct stat *stbuf)
{
  log_debug2("mp3cue_getattr %s", path);
  char *fullpath = make_path(path);
  char *cue = isCueFile(fullpath);

  if (cue != NULL) {
    log_debug2("mp3cue_getattr cue=%s", cue);
    int ret = stat(cue, stbuf);
    PMK_READONLY(stbuf);
    stbuf->st_mode -= S_IFREG;
    stbuf->st_mode += S_IFDIR;
    mc_free(fullpath);
    mc_free(cue);
    DE_MONITOR(
      cue_t *cue=mp3cue_readcue_in_hash(path, false);
      cue_destroy(cue);
    );
    return ret;

  } else {

    data_entry_t *d = datahash_get(DATA, fullpath);
    log_debug2("found d=%p", d);
    if (d != NULL) {
      DE_MONITOR(
        // check if the cuesheet mtime has changed, if so,
        // reread the cue.
        {
          log_debug2("cuefile for %s",fullpath);
          char *cue = getCueFileForTrack(fullpath, true);
          struct stat st;
          int ret = stat(cue, &st);
          log_debug4("stat cuefile %s, mtime=%d, registered:%d",
                      cue,
                      (int) st.st_mtime,
                      (int) d->st->st_mtime
                      );
          mc_free(cue);
          if (st.st_mtime != d->st->st_mtime) {
            char *cpath = getCueFileForTrack(path, false);
            mp3cue_readcue_in_hash(cpath, true); // replace cue in hash
            mc_free(cpath);
          }
        }
        // check if we already have the size
        if (has_size( fullpath, d->st->st_mtime )) {
          log_debug("hassize = true");
          d->st->st_size = get_size( fullpath );
        } else {
          log_debug("hassize = false");
          segmenter_t *s = get_segment(d->entry, true);
          d->st->st_size = segmenter_size(s);
          put_size(fullpath, d->st->st_size, d->st->st_mtime);
        }
        log_debug3("for filename %s, size=%d",
             cue_audio_file(cue_entry_sheet(d->entry)), (int) d->st->st_size);
        memcpy(stbuf, d->st, sizeof(struct stat));
        mc_free(fullpath);
      ); // end monitor
      return 0;
    } else {
      int ret = stat(fullpath, stbuf);
      PMK_READONLY(stbuf);
      mc_free(fullpath);
      return ret;
    }
  }
}

static int mp3cue_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
  log_debug2("mp3cue_readdir %s", path);

  char *fullpath = make_path(path);
  if (fullpath == NULL) {
    return ENOMEM;
  }

  char *cue = isCueFile(fullpath);
  if (cue != NULL) {
    log_debug2("mp3cue_readdir iscuefile %s", cue);
    mc_free(cue);
    mc_free(fullpath);
    int retval;
    DE_MONITOR(
      retval=mp3cue_readcue(path, buf, filler, offset, fi);
    );
    return retval;
  }

  DIR *dh = opendir(fullpath);
  if (dh == NULL) {
    log_debug2("opendir %s returns NULL!", fullpath);
    mc_free(fullpath);
    return errno;
  } else {
    struct dirent *de;
    while ((de = readdir(dh)) != NULL) {
      //log_debug2("d_name=%s",de->d_name);
      if (de->d_name[0] != '.') {
        char *pf = make_path2(path, de->d_name);
        log_debug2("pf=%s", pf);
        struct stat st;
        if (pf == NULL) {
          closedir(dh);
          return ENOMEM;
        } else {
          int r = stat(pf, &st);
          MK_READONLY(st);
          switch (st.st_mode & S_IFMT) {
          case S_IFREG:{
              if (isCue(pf)) {
                char *dr = stripExt(de->d_name, ".cue");
                if (dr == NULL) {
                  closedir(dh);
                  return ENOMEM;
                }
                st.st_mode &= !S_IFREG;
                st.st_mode += S_IFDIR;
                log_debug2("adding cue file %s", dr);
                filler(buf, dr, &st, 0);
                mc_free(dr);
              }
              break;
            }
          case S_IFDIR:{
              //char *dr=make_rel_path2(path,de->d_name);
              //if (dr==NULL) { return ENOMEM; }
              filler(buf, de->d_name, &st, 0);
              //mc_free(dr);
              break;
            }
          default:
            break;
          }
        }
        mc_free(pf);
      } else {
        // skip
      }
    }
    closedir(dh);
  }
  mc_free(fullpath);

  return 0;
}

static int mp3cue_open(const char *path, struct fuse_file_info *fi)
{
  log_debug2("mp3cue_open %s", path);
  char *fullpath = make_path(path);
  char *cue = isCueFile(fullpath);
  if (cue != NULL) {
    int ret = -EISDIR;
    fi->fh = 0;
    mc_free(fullpath);
    mc_free(cue);
    return ret;
  } else {
    data_entry_t *d = datahash_get(DATA, fullpath);
    log_debug2("found d=%p", d);
    if (d != NULL) {
      int retval=0;
      DE_MONITOR(
        int update = cue_entry_audio_changed(d->entry);
        segmenter_t *s = get_segment(d->entry, update );
        if (update) { cue_entry_audio_update_mtime(d->entry); }
        if (segmenter_stream(s) == NULL) {
          if (segmenter_open(s) != SEGMENTER_OK) {
            log_debug2("Cannot open segment %s", cue_entry_vfile(d->entry));
            mc_free(fullpath);
            retval = -EPERM;
          }
        }
        if (retval == 0) {
          FILE *f = segmenter_stream(s);
          fi->fh = fileno(f);
          d->open_count += 1;
          mc_free(fullpath);
        }
      );
      return retval;
    } else {
      fi->fh = 0;
      int ret = -EISDIR;
      mc_free(fullpath);
      return ret;
    }
  }
}

static int mp3cue_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  log_debug4("mp3cue_read %s %d %d", path, (int)size, (int)offset);
  if (fi->fh == 0) {
    return -EIO;
  } else {
    char *fullpath = make_path(path);
    data_entry_t *d = datahash_get(DATA, fullpath);
    mc_free(fullpath);
    log_debug2("found d=%p", d);
    if (d != NULL) {
      DE_MONITOR(
        segmenter_t *s = get_segment(d->entry, false);
        FILE *f = segmenter_stream(s);
      );
      if (f == NULL) {
        return -EIO;
      } else {
        fseek(f, offset, SEEK_SET);
        int bytes = fread(buf, 1, size, f);
        return bytes;
      }
    } else {
      return -EIO;
    }
  }
}

static int mp3cue_release(const char *path, struct fuse_file_info *fi)
{
  log_debug2("mp3cue_release %s", path);
  if (fi->fh == 0) {
    return -EIO;
  } else {
    char *fullpath = make_path(path);
    data_entry_t *d = datahash_get(DATA, fullpath);
    if (d != NULL) {
      log_debug3("found d=%p, count=%d", d, d->open_count);
      d->open_count -= 1;
      if (d->open_count <= 0) {
        d->open_count = 0;
        DE_MONITOR(
          segmenter_t *s = get_segment(d->entry, false);
          log_debug2("closing segment %s", cue_entry_vfile(d->entry));
        );
        segmenter_close(s);
        fi->fh = 0;
      }
      mc_free(fullpath);
      return 0;
    } else {
      mc_free(fullpath);
      return -EIO;
    }
  }
}

static struct fuse_operations mp3cue_oper = {
  .getattr = mp3cue_getattr,
  .readdir = mp3cue_readdir,
  .open = mp3cue_open,
  .read = mp3cue_read,
  .release = mp3cue_release,
};

/***********************************************************************/

extern FILE *log_handle()
{
  static FILE *log = NULL;
  if (log == NULL) {
    log = fopen("/tmp/mp3cue.log", "wt");
  }
  return log;
}

inline extern int log_this_severity(int severity)
{
  //int retval = (severity > LOG_DEBUG);
  int retval=1;
  return retval;
}

/***********************************************************************/

int main(int argc, char *argv[])
{
  // Initialize
  mc_init();

  DATA = datahash_new(100, HASH_CASE_SENSITIVE);
  SEGMENT_LIST = seglist_new();
  SIZE_HASH = vfilesize_hash_new(100, HASH_CASE_SENSITIVE);

  // Read in current sizes
  char *home=getenv("HOME");
  char cfgfile[1024];
  snprintf(cfgfile,1024-1,"%s/.mp3cuefuse",home);
  read_in_sizes(cfgfile);

  // Option handling

  int option_index;
  struct option long_options[] = {
    {"memory", 1, 0, 0},
    {0, 0, 0, 0}
  };

  int c = getopt_long(argc, argv, "m:", long_options, &option_index);
  int _memset = 0;
  if (c >= 0) {
    if (c == 'm') {
      char *memory = optarg;
      MAX_MEM_USAGE_IN_MB = atoi(memory);
      if (MAX_MEM_USAGE_IN_MB < 30) {
        fprintf(stderr, "Defaulting max memory usage to minimum of 30MB\n");
        MAX_MEM_USAGE_IN_MB = 30;
      }
      _memset = 1;
    }
  }

  if (!_memset) {
    fprintf(stderr, "Defaulting max memory usage to 200MB\n");
  } else {
    fprintf(stderr, "Max memory usage set to %dMB\n", MAX_MEM_USAGE_IN_MB);
  }

  int retval = -1;

  if (optind < argc) {
    BASEDIR = mc_strdup(argv[optind++]);
    if (optind < argc) {
      int fargc;
      char **fargv = (char **)mc_malloc(sizeof(char *) * (argc - optind + 2));
      int k = 1;
      fargv[0] = argv[0];
      while (optind < argc) {
        fargv[k++] = argv[optind++];
      }
      fargv[k] = NULL;
      fargc = k;
      log_info("Starting fuse_main");
      retval = fuse_main(fargc, fargv, &mp3cue_oper, NULL);
      log_info2("Retval of fuse_main = %d",retval);
      mc_free(fargv);
    } else {
      retval = usage(argv[0]);
    }
  } else {
    retval = usage(argv[0]);
  }

  // Write out
  write_sizes(cfgfile);

  // Destroy

  log_info("destroying DATA hash");
  datahash_destroy(DATA);
  log_info("destroying SEGMENT_LIST");
  seglist_destroy(SEGMENT_LIST);
  log_info("destroying SIZE_HASH");
  vfilesize_hash_destroy(SIZE_HASH);
  log_info("destroying BASEDIR");
  mc_free(BASEDIR);

  return retval;

}
