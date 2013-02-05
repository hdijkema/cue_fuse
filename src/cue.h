#ifndef __CUE__HOD
#define __CUE__HOD

typedef struct {
	char  *title;
	char  *performer;
	char  *year;
	char  *composer;
	char  *piece;
	int  	tracknr;
	int  	begin_offset_in_ms;
	int  	end_offset_in_ms;
	void  *sheet;
	char  *vfile;
} cue_entry_t;

typedef struct {
	int			 _errno;
	char 		 *audio_file;
	char 		 *album_title;
	char 		 *album_performer;
	char		 *album_composer;
	char        *genre;
	char 		 *cuefile;
	int   		  count;
	cue_entry_t **entries;
} cue_t;


#define ENOCUE 		-1
#define EWRONGCUE 	-2
#define ENOFILECUE  -3

cue_t *cue_new(const char *file);
void  cue_destroy(cue_t *);

int cue_valid(cue_t *);
int cue_errno(cue_t *);

const char *cue_file(cue_t * cue);
const char *cue_album_title(cue_t *cue);
const char *cue_album_performer(cue_t *cue);
const char *cue_album_composer(cue_t *cue);
const char *cue_audio_file(cue_t *cue);
const char *cue_genre(cue_t *cue);

int          cue_entries(cue_t *cue);
cue_entry_t *cue_entry(cue_t *cue,int index);


void cue_entry_destroy(cue_entry_t *ce);    // destroys entry and removes it from cue
                                             // if it is the last one, destroys cue


const char *cue_entry_title(cue_entry_t *ce);
const char *cue_entry_performer(cue_entry_t *ce);
const char *cue_entry_composer(cue_entry_t *ce);
const char *cue_entry_piece(cue_entry_t *ce);
const char *cue_entry_year(cue_entry_t *ce);
int			 cue_entry_tracknr(cue_entry_t *ce);
int			 cue_entry_begin_offset_in_ms(cue_entry_t *ce);
int   		 cue_entry_end_offset_in_ms(cue_entry_t *ce);
cue_t       *cue_entry_sheet(cue_entry_t *ce);
const char *cue_entry_vfile(cue_entry_t *ce);
char        *cue_entry_alloc_id(cue_entry_t *ce);

#endif


