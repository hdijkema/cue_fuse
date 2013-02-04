#include "cue.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"

#define T(a) 	(a==NULL) ? "" : a

static char *mystrdup(const char *s) {
	if (s==NULL) { 
		return NULL;
	} else {
		return strdup(s);
	}
}

static char *readline(FILE *f) {
	char *line=mystrdup("");
	char  buf[10240];
	int   readsome=0;
	
	while(fgets(buf,10240,f)!=NULL) {
		readsome=1;
		{
			int i,ret=0;
			for(i=0;buf[i]!='\0' && buf[i]!='\n';i++);
			if (buf[i]=='\n') { buf[i]='\0';ret=1; }
			line=(char *) realloc(line,strlen(line)+1+i+1);
			strcat(line,buf);
			if (ret) {
				return line;
			}
		}
	}
	
	if (readsome) {
		return line;
	} else {
		free(line);
		return NULL;
	}
}

static char *trim(const char *line) {
	int i,j;
	for(i=0;line[i]!='\0' && isspace(line[i]);i++);
	char *k=mystrdup(&line[i]);
	for(j=strlen(k)-1;j>=0 && isspace(k[j]);j--);
	if (j>=0) {
		k[j+1]='\0';
	}
	return k;
}

static void trim_replace(char **line) {
	char *rl=trim(*line);
	free(*line);
	*line=rl;
}

static int eq(const char *s,const char *e) {
	char *r=trim(s);
	if (strncasecmp(r,e,strlen(e))==0) {
		free(r);
		return 1;
	} else {
		free(r);
		return 0;
	}
}

static char *unquote(const char *s,const char *e) {
	char *r=trim(s);
	char *p=&r[strlen(e)];
	while (isspace(p[0]) && p[0]!='\0') { p++; }
	if (p[0]=='\0') {
		return mystrdup("");
	} else {
		if (p[0]=='"') { p+=1; }
		if (p[strlen(p)-1]=='"') { p[strlen(p)-1]='\0'; }
		char *k=mystrdup(p);
		trim_replace(&k);
		free(r);
		log_debug2("k=%s",k);
		return k;
	}	
}

static char *getFilePart(const char *s) {
	int i,j;
	for(i=0;s[i]!='"' && s[i]!='\0';i++);
	char *fl=strdup(&s[i]);
	for(i=strlen(fl)-1;i>=0 && fl[i]!='"';i--);
	fl[i+1]='\0';
	return fl;
}

static cue_entry_t *cue_entry_new(cue_t *s) {
	cue_entry_t *r=(cue_entry_t *) malloc(sizeof(cue_entry_t));
	if (r==NULL) { return NULL; }
	r->title=NULL;
	r->performer=NULL;
	r->year=NULL;
	r->composer=NULL;
	r->tracknr=-1;
	r->begin_offset_in_ms=-1;
	r->end_offset_in_ms=-1;
	r->sheet=(void *) s;
	r->vfile=NULL;
	return r;
}

static void addEntry(cue_t *r,cue_entry_t *entry) {
	r->count+=1;
	r->entries=(cue_entry_t **) realloc(r->entries,sizeof(cue_entry_t *)*r->count);
	r->entries[r->count-1]=entry;
}

static int calculateOffset(const char *in) {
	int i,N;
	for(i=0,N=strlen(in);isspace(in[i]);i++);
	if (in[i]>='0' && in[i]<='9') {
		for(;in[i]>='0' && in[i]<='9';i++);
		if (in[i]!='\0') {
			for(;isspace(in[i]);i++);
			if (in[i]>='0' && in[i]<='9') {
				const char *min=&in[i];
				for(;in[i]!=':' && in[i]!='\0';i++);
				if (in[i]=='\0') { return -1; }
				i+=1;
				const char *sec=&in[i];
				for(;in[i]!=':' && in[i]!='\0';i++);
				if (in[i]=='\0') { return -1; }
				i+=1;
				const char *hs=&in[i];
				
				{
					int m=atoi(min);
					int s=atoi(sec);
					int ms=atoi(hs)*10;
					return m*60*1000+s*1000+ms;
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

static char *getExt(const char *filename) {
	int i=strlen(filename)-1;
	for(;i>=0 && filename[i]!='.';i--);
	if (i<0) { return mystrdup(""); }
	else { return mystrdup(&filename[i+1]); }
}

/**************************************************************/

cue_t *cue_new(const char *file) {
	cue_t *r=(cue_t *) malloc(sizeof(cue_t));

	r->audio_file		= NULL;
	r->album_title		= NULL;
	r->album_performer	= NULL;
	r->album_composer   = NULL;
	r->genre            = NULL;
	r->cuefile			= mystrdup(file);
	r->count			= 0;
	r->entries			= NULL;
	r->_errno			= 0;

	FILE *f=fopen(file,"rt");
	if (f==NULL) {
		r->_errno=ENOFILECUE;
	} else {
		char *line;
		char *image=NULL;
		char *year=NULL;
		cue_entry_t *entry=NULL;
		int   in_tracks=0;
		while ((line=readline(f))!=NULL) {
			log_debug2("reading line: %s",line);
			trim_replace(&line);
			log_debug2("reading line: %s",line);
			if (strcmp(line,"")!=0) {
				if (!in_tracks) {
					if (eq(line,"performer")) {
						free(r->album_performer);
						r->album_performer=unquote(line,"performer");
					} else if (eq(line,"title")) {
						free(r->album_title);
						r->album_title=unquote(line,"title");
					} else if (eq(line,"file")) {
						free(r->audio_file);
						char *fl=getFilePart(line);
						char *af=unquote(fl,"");
						if (strlen(af)>0) {
							if (af[0]=='/') { r->audio_file=af; }
							else {
								char *cf=strdup(r->cuefile);
								int ii;
								for(ii=strlen(cf)-1;ii>=0 && cf[ii]!='/';ii--);
								if (ii>=0) {
									cf[ii]='\0';
									char *aaf=(char *) malloc(strlen(cf)+strlen(af)+strlen("/")+1);
									sprintf(aaf,"%s/%s",cf,af);
									r->audio_file=aaf;
									free(cf);
								} else {
									r->audio_file=af;
								}
							}
						} else {
							r->audio_file=af;
						}	
						free(fl);
					} else if (eq(line,"rem")) {
						if (eq(&line[3],"date")) {
							free(year);
							year=unquote(&line[3],"date");
						} else if (eq(&line[3],"image")) {
							free(image);
							image=unquote(&line[3],"image");
						} else if (eq(&line[3],"composer")) {
							free(r->album_composer);
							r->album_performer=unquote(&line[3],"composer");
						} else if (eq(&line[3],"genre")) {
							free(r->genre);
							r->genre=unquote(&line[3],"genre");
						}
					} else if (eq(line,"track")) {
						in_tracks=1;
					}
				}
				
				if (in_tracks) {
					if (eq(line,"track")) {
						log_debug2("track: entry=%p",entry);
						if (entry!=NULL) {
							addEntry(r,entry);
						} 
						entry=cue_entry_new(r);
						entry->year=mystrdup(year);
						entry->performer=mystrdup(r->album_performer);
						entry->composer=mystrdup(r->album_composer);
						entry->piece=NULL;
						log_debug2("track: created new entry %p",entry);
					} else if (eq(line,"title")) {
						free(entry->title);
						entry->title=unquote(line,"title");
					} else if (eq(line,"performer")) {
						free(entry->performer);
						entry->performer=unquote(line,"performer");
					} else if (eq(line,"index")) {
						char *index=unquote(line,"index");
						entry->begin_offset_in_ms=calculateOffset(index);
						free(index);
					} else if (eq(line,"rem")) {
						if (eq(&line[3],"composer")) {
							free(entry->composer);
							entry->composer=unquote(&line[3],"composer");
						} else if (eq(&line[3],"piece")) {
							free(entry->piece);
							entry->piece=unquote(&line[3],"piece");
						} else if (eq(&line[3],"year")) {
							free(year);
							year=unquote(&line[3],"year");
							free(entry->year);
							entry->year=mystrdup(year);
						}
					}
				}
			}
			free(line);
		}
		if (entry!=NULL) {
			addEntry(r,entry);
		}
		free(year);
		free(image);
		
		{
			int i,N;
			for(i=0,N=r->count;i<N-1;i++) {
				r->entries[i]->end_offset_in_ms=r->entries[i+1]->begin_offset_in_ms;
				r->entries[i]->tracknr=i+1;
			}
			r->entries[i]->tracknr=i+1;
		}
		
	}

	return r;
}

void cue_destroy(cue_t *c) {
	free(c->audio_file);
	free(c->album_title);
	free(c->album_performer);
	free(c->album_composer);
	free(c->genre);
	free(c->cuefile);
	
	{
		int i,N;
		for(i=0,N=c->count;i<N;i++) {
			cue_entry_t *e=c->entries[i];
			free(e->title);
			free(e->performer);
			free(e->year);
			free(e->composer);
			free(e->piece);
			free(e);
		}
	}	
}

int cue_valid(cue_t *c) {
	return c->_errno==0;
}

int cue__errno(cue_t *c) {
	return c->_errno;
}

const char *cue_file(cue_t * cue) {
	return T(cue->cuefile);
}

const char *cue_album_title(cue_t *cue) {
	return T(cue->album_title);
}

const char *cue_album_performer(cue_t *cue) {
	return T(cue->album_performer);
}

const char *cue_album_composer(cue_t *cue) {
	return T(cue->album_composer);
}

const char *cue_genre(cue_t *cue) {
	return T(cue->genre);
}

const char *cue_audio_file(cue_t *cue) {
	return T(cue->audio_file);
}

int cue_count(cue_t *cue) {
	return cue->count;
}

cue_entry_t *cue_entry(cue_t *cue,int index) {
	if (index<0) { return NULL; }
	else if (index>=cue->count) { return NULL; }
	else { return cue->entries[index]; }
}

const char *cue_entry_title(cue_entry_t *ce) {
	return T(ce->title);
}

const char *cue_entry_performer(cue_entry_t *ce) {
	return T(ce->performer);
}

const char *cue_entry_composer(cue_entry_t *ce) {
	return T(ce->composer);
}

const char *cue_entry_piece(cue_entry_t *ce) {
	return T(ce->piece);
}

const char *cue_entry_year(cue_entry_t *ce) {
	return T(ce->year);
}

int	cue_entry_tracknr(cue_entry_t *ce) {
	return ce->tracknr;
}

int	cue_entry_begin_offset_in_ms(cue_entry_t *ce) {
	return ce->begin_offset_in_ms;
}

int cue_entry_end_offset_in_ms(cue_entry_t *ce) {
	return ce->end_offset_in_ms;
}

cue_t *cue_entry_sheet(cue_entry_t *ce) {
	return (cue_t *) ce->sheet;
}

const char *cue_entry_vfile(cue_entry_t *ce) {
	if (ce->vfile==NULL) {
		cue_t *c=(cue_t *) ce->sheet;
		char *name=(char *) malloc(10+strlen(cue_entry_title(ce))+4);
		char *ext=getExt(cue_audio_file(c));
		sprintf(name,"%02d - %s.%s",ce->tracknr,ce->title,ext);
		free(ext);
		ce->vfile=name;
	}
	return ce->vfile;
}

char *cue_entry_alloc_id(cue_entry_t *ce) {
	int l=strlen(cue_entry_vfile(ce))+strlen(cue_audio_file(cue_entry_sheet(ce)))+1;
	char *s=(char *) malloc(l);
	strcpy(s,cue_entry_vfile(ce));
	strcat(s,cue_audio_file(cue_entry_sheet(ce)));
	return s;
}




