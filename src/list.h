#ifndef __LIST__HOD
#define __LIST__HOD

#include <pthread.h>

typedef void * list_data_t;

typedef struct __list_entry__ {
	struct __list_entry__ *next;
	struct __list_entry__ *previous;
	void * data;
} list_entry_t;

typedef struct {
	pthread_mutex_t *mutex;
	list_entry_t    *first;
	list_entry_t    *last;
	list_entry_t    *current;
	int count;
} list_t;

typedef enum { LIST_LAST, LIST_FIRST } list_pos_t;

list_t *  _list_new();
void      _list_destroy(list_t *,void (*data_destroyer)(list_data_t v));
int       _list_length(list_t *);
void      _list_lock(list_t *);
void      _list_unlock(list_t *);

list_data_t _list_start_iter(list_t *,list_pos_t pos);
list_data_t _list_iter_at(list_t *,int i);
list_data_t _list_next_iter(list_t *);
list_data_t _list_prev_iter(list_t *);

void         _list_drop_iter(list_t *,void (*data_destroyer)(list_data_t v));
void         _list_prepend_iter(list_t *,list_data_t data);
void         _list_append_iter(list_t *,list_data_t data);
void		  _list_move_iter(list_t *,list_pos_t pos);



#define DECLARE_LIST(NAME,T) \
  typedef list_t  NAME; \
  NAME * NAME##_new(); \
  void     NAME##_destroy(NAME *l); \
  int      NAME##_length(NAME *l); \
  int      NAME##_count(NAME *l); \
  T *      NAME##_start_iter(NAME *l,list_pos_t p); \
  T *      NAME##_next_iter(NAME *l); \
  T *      NAME##_prev_iter(NAME *l); \
  void     NAME##_drop_iter(NAME *l); \
  void     NAME##_prepend_iter(NAME *l,T *e); \
  void     NAME##_append_iter(NAME *l, T *e); \
  T *      NAME##_iter_at(NAME *l, int i); \
  void     NAME##_move_iter(NAME *l,list_pos_t pos); \
  void     NAME##_lock(NAME *l); \
  void     NAME##_unlock(NAME *l); 

#define IMPLEMENT_LIST(NAME,T,COPY,DESTROY) \
  NAME * NAME##_new() { return _list_new(); } \
  void     NAME##_destroy(NAME *l) { _list_destroy(l,DESTROY); } \
  int      NAME##_length(NAME *l) { return _list_length(l); } \
  int      NAME##_count(NAME *l) { return _list_length(l); } \
  T *      NAME##_start_iter(NAME *l,list_pos_t p) { return (T *) _list_start_iter(l,p); } \
  T *      NAME##_next_iter(NAME *l) { return (T *) _list_next_iter(l); } \
  T *      NAME##_prev_iter(NAME *l) { return (T *) _list_prev_iter(l); } \
  void     NAME##_drop_iter(NAME *l) { _list_drop_iter(l,DESTROY); } \
  void     NAME##_prepend_iter(NAME *l,T *e) { _list_prepend_iter(l,COPY(e)); } \
  void     NAME##_append_iter(NAME *l, T *e) { _list_append_iter(l,COPY(e)); } \
  T *      NAME##_iter_at(NAME *l, int i) { return (T *) _list_iter_at(l,i); } \
  void     NAME##_move_iter(NAME *l,list_pos_t pos) { _list_move_iter(l,pos); } \
  void     NAME##_lock(NAME *l) { _list_lock(l); } \
  void     NAME##_unlock(NAME *l) { _list_unlock(l); }

#endif
