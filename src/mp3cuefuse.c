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
#include "hash.h"
#include "cue.h"
#include "segmenter.h"
#include "log.h"
#include "list.h"

/***********************************************************************/

static char 		*BASEDIR;
static int          MAX_MEM_USAGE_IN_MB=200;

/***********************************************************************/

int usage(char *p) {
	fprintf(stderr,
			"%s [--memory|m maxMB] <cue directory> <mountpoint>\n",
			p);
	return 1;
}

/***********************************************************************/

typedef struct {
	char         *id;
	segmenter_t *segment;
} seg_entry_t;

list_data_t seg_entry_copy(seg_entry_t *e) {
	return (list_data_t) e;
}

void seg_entry_destroy(list_data_t _e) {
	seg_entry_t *e=(seg_entry_t *) _e;
	free(e->id);
	segmenter_destroy(e->segment);
	free(e);
}

DECLARE_LIST(seglist,seg_entry_t);
IMPLEMENT_LIST(seglist,seg_entry_t,seg_entry_copy,seg_entry_destroy);

static list_t *SEGMENT_LIST=NULL;

void add_seg_entry(cue_entry_t *e,segmenter_t *s) {
	log_debug("lock segment list");
	seglist_lock(SEGMENT_LIST);
	{
		log_debug("first look if we need to destroy a segment");
		seg_entry_t *se;
		se=seglist_start_iter(SEGMENT_LIST,LIST_FIRST);
		double count_mb=0.0;
		while (se!=NULL) {
			count_mb+=segmenter_size(se->segment)/(1024.0*1024.0);
			se=seglist_next_iter(SEGMENT_LIST);
		}
		log_debug("drop last ones as long we're above our memory limit");
		int n=seglist_count(SEGMENT_LIST);
		int k=0;
		while (((int) count_mb)>MAX_MEM_USAGE_IN_MB && k<5) {
			se=seglist_start_iter(SEGMENT_LIST,LIST_LAST);
			if (se!=NULL) {
				if (segmenter_stream(se->segment)==NULL) {
					count_mb-=segmenter_size(se->segment)/(1024.0*1024.0);
					seglist_drop_iter(SEGMENT_LIST);
					k=0;
				} else {
					seglist_move_iter(SEGMENT_LIST,LIST_FIRST);
					k+=1;
				}
			} else {
				count_mb=0;
			}
		}
		log_debug("add our segment on front");
		se=(seg_entry_t *) malloc(sizeof(seg_entry_t));
		se->id=cue_entry_alloc_id(e);
		se->segment=s;
		seglist_start_iter(SEGMENT_LIST,LIST_FIRST);
		seglist_prepend_iter(SEGMENT_LIST,se);
	}
	log_debug("unlock segmentlist");
	seglist_unlock(SEGMENT_LIST);
}

segmenter_t *find_seg_entry(cue_entry_t *e) {
	char *id=cue_entry_alloc_id(e);
	seglist_lock(SEGMENT_LIST);
	seg_entry_t *se=seglist_start_iter(SEGMENT_LIST,LIST_FIRST);
	while (se!=NULL && strcmp(se->id,id)!=0) {
		se=seglist_next_iter(SEGMENT_LIST);
	}
	log_debug3("found segment %p for id %s",se,id);
	free(id);
	seglist_unlock(SEGMENT_LIST);
	if (se==NULL) {
		return NULL;
	} else {
		return se->segment;
	}
}

/***********************************************************************/

typedef struct {
	cue_entry_t 	*entry;
	char       		*path;
	struct stat 	*st;
	int              open_count;
} data_entry_t;

static data_entry_t *data_entry_new(const char *path,cue_entry_t *entry,struct stat *st) {
	data_entry_t *e=(data_entry_t *) malloc(sizeof(data_entry_t));
	e->path=strdup(path);
	e->entry=entry;
	e->open_count=0;
	if (st!=NULL) {
		struct stat *stn=(struct stat *) malloc(sizeof(struct stat));
		memcpy((void *) stn,(void *) st,sizeof(struct stat));
		e->st=stn;
	} else {
		e->st=NULL;
	}
}

static void data_entry_destroy(data_entry_t *e) {
    log_debug2("Destroying cue entry %s",cue_entry_title(e->entry));
    cue_entry_destroy(e->entry);
	free(e->path);
	free(e->st);
}

static hash_data_t data_copy(data_entry_t *e) {
    return (hash_data_t) e;
}

static void data_destroy(hash_data_t d) {
    data_entry_t *e=(data_entry_t *) d;
    data_entry_destroy(e);
}

DECLARE_HASH(datahash,data_entry_t);
IMPLEMENT_HASH(datahash,data_entry_t,data_copy,data_destroy);

datahash *DATA=NULL;

/***********************************************************************/

char * make_path(const char *path) {
	int l=strlen(path)+strlen(BASEDIR)+1;
	char *np=(char *) malloc(l);
	fprintf(stderr,"np=%p\n",np);
	if (np==NULL) {
		return np;
	} else {
		strcpy(np,BASEDIR);
		strcat(np,path);
		log_debug2("fullpath=%s",np);
		//fprintf(stderr,"fullpath=%s\n",np);
		return np;
	}
}

char * make_rel_path2(const char *path,const char *file) {
	int pl=strlen(path);
	int l=strlen(path)+strlen("/")+strlen(file)+1;
	char *fp=(char *) malloc(l);
	if (fp==NULL) {
		return NULL;
	} else {
		strcpy(fp,path);
		if (pl>0) {
			if (fp[pl-1]!='/') {
				strcat(fp,"/");
			}
		}
		strcat(fp,file);
		return fp;
	}
}

char * make_path2(const char *path,const char *file) {
	char *np=make_path(path);
	if (np==NULL) {
		return np;
	} else {
		char *r=make_rel_path2(np,file);
		free(np);
		return r;
	}
}

static int isExt(const char *path,const char *ext) {
	int l=strlen(ext);
	int pl=strlen(path);
	if (pl>=l) {
		if (strcasecmp(path+pl-l,ext)==0) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

static int isCue(const char *path) {
	return isExt(path,".cue");
}

static int isImage(const char *path) {
	return isExt(path,".jpg") || isExt(path,".jpeg") || isExt(path,".png");
}

static char *stripExt(const char *_path,const char *ext) {
	char *path=strdup(_path);
	if (path==NULL) {
		return NULL;
	} else if (isExt(path,ext)) {
		int l=strlen(path)-strlen(ext);
		if (l>0) {
			path[l]='\0';
		}
	}
	return path;
}

static char *isCueFile(const char *full_path) {
	char *fp=(char *) malloc(strlen(full_path)+strlen(".cue")+1);
	char *cues[]={".cue",".Cue",".cUe",".cuE",".CUe",".CuE",".cUE",".CUE",NULL};
	int i;

	for(i=0;cues[i]!=NULL;i++) {
		sprintf(fp,"%s%s",full_path,cues[i]);
		FILE *f=fopen(fp,"r");
		if (f!=NULL) {
			fclose(f);
			return fp;
		}
	}

	free(fp);

	return NULL;
}

/***********************************************************************/

static segmenter_t *create_segment(cue_entry_t *e) {
	segmenter_t *se=find_seg_entry(e);
	if (se!=NULL) {
		return se;
	} else {
		cue_t *sheet=cue_entry_sheet(e);
		segmenter_t *s=segmenter_new();
		const char *fullpath=cue_audio_file(sheet);
		int year=atoi(cue_entry_year(e));
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
						  cue_entry_piece(e),
						  cue_entry_begin_offset_in_ms(e),
						  cue_entry_end_offset_in_ms(e)
						  );
		segmenter_create(s);
		add_seg_entry(e,s);
		return s;
	}
}

/***********************************************************************/

static  list_data_t delist_copy(data_entry_t *e) {
    return (list_data_t) e;
}

static void delist_destroy_entry(list_data_t e) {
    data_entry_destroy((data_entry_t *) e);
}

DECLARE_LIST(delist,data_entry_t);
IMPLEMENT_LIST(delist,data_entry_t,delist_copy,delist_destroy_entry);

static cue_t * mp3cue_readcue_in_btree(const char *path) {
	char *fullpath=make_path(path);
	char *cuefile=isCueFile(fullpath);
	log_debug2("reading cuefile %s",cuefile);
	cue_t *cue=cue_new(cuefile);
	struct stat st;
	stat(cuefile,&st);
	if (cue!=NULL && cue_valid(cue)) {
	    int added=0;
		int i,N;
		// check if cue already exists in hash. If not, create whole cue in hash and read in again.
		for(i=0,N=cue_count(cue);i<N;i++) {
			cue_entry_t *entry=cue_entry(cue,i);
			char *p=make_path2(path,cue_entry_vfile(entry));
			log_debug2("p=%s",p);
			data_entry_t *d=data_entry_new(p,entry,&st);
			if (datahash_exists(DATA,p)) {
                // do nothing
			} else {
                datahash_put(DATA,p,d);
                added=1;
            }
		}
		if (added) {
		    free(cuefile);
		    free(fullpath);
		    return mp3cue_readcue_in_btree(path);
		}
	}

	free(cuefile);
	free(fullpath);
	return cue;
}

static int mp3cue_readcue(const char *path, void *buf, fuse_fill_dir_t filler,
						   off_t offset, struct fuse_file_info *fi)
{
	log_debug2("enter with %s",path);
	cue_t *cue=mp3cue_readcue_in_btree(path);

	char *fullpath=make_path(path);
	char *cuefile=isCueFile(fullpath);

	struct stat st;
	stat(cuefile,&st);

	if (cue!=NULL && cue_valid(cue)) {
		int i,N;
		for(i=0,N=cue_count(cue);i<N;i++) {
			cue_entry_t *entry=cue_entry(cue,i);
			data_entry_t *d=datahash_get(DATA,fullpath);
			if (d!=NULL) {
				filler(buf,cue_entry_vfile(entry),d->st,0);
			} else {
				filler(buf,cue_entry_vfile(entry),&st,0);
			}
		}
	}

	cue_destroy(cue);

	free(cuefile);
	free(fullpath);

	return 0;
}


/***********************************************************************/

static int mp3cue_getattr(const char *path, struct stat *stbuf)
{
	log_debug2("mp3cue_getattr %s",path);
	char *fullpath=make_path(path);
	char *cue=isCueFile(fullpath);
	if (cue!=NULL) {
		log_debug2("mp3cue_getattr cue=%s",cue);
		int ret=stat(cue,stbuf);
		stbuf->st_mode-=S_IFREG;
		stbuf->st_mode+=S_IFDIR;
		free(fullpath);
		free(cue);
		mp3cue_readcue_in_btree(path);
		return ret;
	} else {
		data_entry_t *d=datahash_get(DATA,fullpath);
		log_debug2("found d=%p",d);
		if (d!=NULL) {
			segmenter_t *s=create_segment(d->entry);
			d->st->st_size=segmenter_size(s);
			log_debug3("for filename %s, size=%d",cue_audio_file(cue_entry_sheet(d->entry)),(int) segmenter_size(s));
			memcpy(stbuf,d->st,sizeof(struct stat));
			free(fullpath);
			return 0;
		} else {
			int ret=stat(fullpath,stbuf);
			free(fullpath);
			return ret;
		}
	}
}



static int mp3cue_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	                       off_t offset, struct fuse_file_info *fi)
{
	log_debug2("mp3cue_readdir %s",path);

	char *fullpath=make_path(path);
	if (fullpath==NULL) {
		return ENOMEM;
	}

	char *cue=isCueFile(fullpath);
	if (cue!=NULL) {
		log_debug2("mp3cue_readdir iscuefile %s",cue);
		free(cue);
		return	mp3cue_readcue(path,buf,filler,offset,fi);
	}

	DIR *dh=opendir(fullpath);
	if (dh==NULL) {
		free(fullpath);
		return errno;
	} else {
		struct dirent *de;
		while ((de=readdir(dh))!=NULL) {
			//log_debug2("d_name=%s",de->d_name);
			if (de->d_name[0]!='.') {
				char *pf=make_path2(path,de->d_name);
				log_debug2("pf=%s",pf);
				struct stat st;
				if (pf==NULL) {
					return ENOMEM;
				} else {
					int r=stat(pf,&st);
					switch (st.st_mode & S_IFMT) {
						case S_IFREG: {
								if (isCue(pf)) {
									char *dr=stripExt(de->d_name,".cue");
									if (dr==NULL) { return ENOMEM; }
									st.st_mode&=!S_IFREG;
									st.st_mode+=S_IFDIR;
									log_debug2("adding cue file %s",dr);
									filler(buf,dr,&st,0);
									free(dr);
								}
								break;
						}
						case S_IFDIR: {
								//char *dr=make_rel_path2(path,de->d_name);
								//if (dr==NULL) { return ENOMEM; }
								filler(buf,de->d_name,&st,0);
								//free(dr);
								break;
						}
						default:
								break;
					}
				}
				free(pf);
			} else {
				// skip
			}
		}
		closedir(dh);
	}

	return 0;
}

static int mp3cue_open(const char *path, struct fuse_file_info *fi)
{
	log_debug2("mp3cue_open %s",path);
	char *fullpath=make_path(path);
	char *cue=isCueFile(fullpath);
	if (cue!=NULL) {
		int ret=-EISDIR;
		fi->fh=0;
		free(fullpath);
		free(cue);
		return ret;
	} else {
		data_entry_t *d=datahash_get(DATA,fullpath);
		log_debug2("found d=%p",d);
		if (d!=NULL) {
			segmenter_t *s=create_segment(d->entry);
			if (segmenter_stream(s)==NULL) {
				if (segmenter_open(s)!=SEGMENTER_OK) {
					free(fullpath);
					return -EPERM;
				}
			}
			FILE *f=segmenter_stream(s);
			fi->fh=fileno(f);
			d->open_count+=1;
			free(fullpath);
			return 0;
		} else {
			fi->fh=0;
			int ret=-EISDIR;
			free(fullpath);
			return ret;
		}
	}
}

static int mp3cue_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
	log_debug4("mp3cue_read %s %d %d",path,(int) size,(int) offset);
	if (fi->fh==0) {
		return -EIO;
	} else {
		char *fullpath=make_path(path);
		data_entry_t *d=datahash_get(DATA,fullpath);
		log_debug2("found d=%p",d);
		if (d!=NULL) {
			segmenter_t *s=create_segment(d->entry);
			FILE *f=segmenter_stream(s);
			if (f==NULL) {
				return -EIO;
			} else {
				fseek(f,offset,SEEK_SET);
				int bytes=fread(buf,1,size,f);
				return bytes;
			}
		} else {
			return -EIO;
		}
	}
}

static int mp3cue_release(const char *path,struct fuse_file_info *fi)
{
	log_debug2("mp3cue_release %s",path);
	if (fi->fh==0) {
		return -EIO;
	} else {
		char *fullpath=make_path(path);
		data_entry_t *d=datahash_get(DATA,fullpath);
		if (d!=NULL) {
		    log_debug3("found d=%p, count=%d",d, d->open_count);
		    d->open_count-=1;
		    if (d->open_count<=0) {
                d->open_count=0;
                segmenter_t *s=create_segment(d->entry);
                segmenter_close(s);
                fi->fh=0;
		    }
			return 0;
		} else {
			return -EIO;
		}
	}
}

static struct fuse_operations mp3cue_oper = {
  .getattr  = mp3cue_getattr,
  .readdir 	= mp3cue_readdir,
  .open   	= mp3cue_open,
  .read   	= mp3cue_read,
  .release  = mp3cue_release,
};

extern FILE *log_handle() {
	static FILE *log=NULL;
	if (log==NULL) {
		log=fopen("/tmp/mp3cue.log","wt");
	}
	return log;
}


int main(int argc, char *argv[])
{
	// Initialize
	DATA=datahash_new(100,HASH_CASE_SENSITIVE);
	SEGMENT_LIST=seglist_new();

	// Option handling

	int option_index;
	struct option long_options[] = {
			{ "memory", 1, 0, 0 },
			{ 0, 0, 0, 0 }
	};

	int c=getopt_long(argc,argv,"m:",long_options,&option_index);
	int _memset=0;
	if (c>=0) {
		if (c=='m') {
			char *memory=optarg;
			MAX_MEM_USAGE_IN_MB=atoi(memory);
			if (MAX_MEM_USAGE_IN_MB<30) {
				fprintf(stderr,"Defaulting max memory usage to minimum of 30MB\n");
				MAX_MEM_USAGE_IN_MB=30;
			}
			_memset=1;
		}
	}

	if (!_memset) {
		fprintf(stderr,"Defaulting max memory usage to 200MB\n");
	} else {
		fprintf(stderr,"Max memory usage set to %dMB\n",MAX_MEM_USAGE_IN_MB);
	}

    int retval=-1;

	if (optind < argc) {
		BASEDIR=strdup(argv[optind++]);
		if (optind < argc) {
			int fargc;
			char **fargv=(char **) malloc(sizeof(char *)*(argc-optind+2));
			int k=1;
			fargv[0]=argv[0];
			while(optind<argc) {
			  fargv[k++]=argv[optind++];
			}
			fargv[k]=NULL;
			fargc=k;
            retval=fuse_main(fargc, fargv, &mp3cue_oper, NULL);
		} else {
            retval=usage(argv[0]);
		}
	} else {
        retval=usage(argv[0]);
	}

    // Destroy

    datahash_destroy(DATA);
    seglist_destroy(SEGMENT_LIST);

    return retval;

}

