#include "list.h"
#include <malloc.h>
#include <assert.h>
#include "log.h"

list_t *  _list_new()  {
	list_t *l=(list_t *) malloc(sizeof(list_t));
	l->first=NULL;
	l->last=NULL;
	l->current=NULL;
	l->mutex=(pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(l->mutex,NULL);
	return l;
}

void _list_destroy(list_t *l,void (*data_destroyer)(list_data_t v)) {
	log_assert(l!=NULL);
	list_entry_t *e=l->first;
	while (e!=NULL) {
		data_destroyer(e->data);
		list_entry_t *next=e->next;
		free(e);
		e=next;
	}
	pthread_mutex_destroy(l->mutex);
	free(l);
}

void _list_lock(list_t *l) {
	log_assert(l!=NULL);
	pthread_mutex_lock(l->mutex);
}

void _list_unlock(list_t *l) {
	log_assert(l!=NULL);
	pthread_mutex_unlock(l->mutex);
}

int _list_length(list_t *l) {
	log_assert(l!=NULL);
	return l->count;
}

list_data_t _list_start_iter(list_t *l,list_pos_t pos) {
	log_assert(l!=NULL);
	if (pos==LIST_FIRST) {
		l->current=l->first;
	} else {
		l->current=l->last;
	} 
	if (l->current==NULL) {
		return NULL;
	} else {
		return l->current->data;
	}
}

list_data_t _list_iter_at(list_t *l,int i) {
	log_assert(l!=NULL);
	if (i>=l->count) {
		return NULL;
	} else if (i<0) {
		return NULL;
	} else {
		l->current=l->first;
		while(i>0) { l->current=l->current->next;i-=1; }
		return l->current->data;
	}
}

list_data_t _list_next_iter(list_t *l) {
	log_assert(l!=NULL);
	if (l->current==NULL) {
		return NULL;
	} else {
		l->current=l->current->next;
		if (l->current==NULL) {
			return NULL;
		} else {
			return l->current->data;
		}
	}
}

list_data_t _list_prev_iter(list_t *l) {
	log_assert(l!=NULL);
	if (l->current==NULL) {
		return NULL;
	} else {
		l->current=l->current->previous;
		if (l->current==NULL) {
			return NULL;
		} else {
			return l->current->data;
		}
	}
}

void _list_drop_iter(list_t *l,void (*data_destroyer)(list_data_t v)) {
	log_assert(l!=NULL);
	if (l->current!=NULL) {
		list_entry_t *e=l->current;
		if (e==l->last && e==l->first) {
			l->current=NULL;
			l->first=NULL;
			l->last=NULL;
		} else if (e==l->last) {
			l->current=NULL;
			l->last=l->last->previous;
			l->last->next=NULL;
		} else if (e==l->first) {
			l->first=e->next;
			l->first->previous=NULL;
			e->previous=NULL;
			l->current=e->next;
		} else {
			e->previous->next=e->next;
			e->next->previous=e->previous;
			l->current=e->next;
		}
		l->count-=1;
		data_destroyer(e->data);
		free(e);
	}
}

void _list_prepend_iter(list_t *l ,list_data_t data) {
	log_assert(l!=NULL);
	if (l->current==NULL) {
		if (l->first==NULL && l->last==NULL) { // first entry in the list
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->next=NULL;
			e->previous=NULL;
			e->data=data;
			l->first=e;
			l->last=e;
			l->current=e;
			l->count=1;
		} else { // prepend before the list
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->next=l->first;
			l->first->previous=e;
			e->previous=NULL;
			e->data=data;
			l->first=e;
			l->current=e;
			l->count+=1;
		}
	} else {
		if (l->current==l->first) {
			l->current=NULL;
			_list_prepend_iter(l,data);
		} else if (l->current==l->last) {
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->previous=l->last;
			l->last->next=e;
			e->next=NULL;
			e->data=data;
			l->last=e;
			l->current=e;
			l->count+=1;
		} else {
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->previous=l->current->previous;
			e->next=l->current;
			e->previous->next=e;
			e->next->previous=e;
			e->data=data;
			l->current=e;
			l->count+=1;
		}
	}
}

void _list_append_iter(list_t *l,list_data_t data) {
	log_assert(l!=NULL);
	if (l->current==NULL) {
		if (l->first==NULL && l->last==NULL) {
			_list_prepend_iter(l,data);
		} else { // append at the end of list
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->next=NULL;
			e->previous=l->last;
			e->data=data;
			l->last->next=e;
			l->last=e;
			l->current=e;
			l->count+=1;
		}
	} else {
		if (l->current==l->first && l->current==l->last) {
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->previous=l->first;
			e->next=l->first->next;
			l->first->next=e;
			l->last=e;
			e->data=data;
			l->current=e;
			l->count+=1;
		} else if (l->current==l->first) {
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->previous=l->first;
			e->next=l->first->next;
			l->first->next=e;
			e->next->previous=e;
			e->data=data;
			l->current=e;
			l->count+=1;
		} else if (l->current==l->last) {
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->previous=l->last;
			e->next=NULL;
			e->previous->next=e;
			e->data=data;
			l->last=e;
			l->current=e;
			l->count+=1;
		} else {
			list_entry_t *e=(list_entry_t *) malloc(sizeof(list_entry_t));
			e->previous=l->current;
			e->next=l->current->next;
			e->previous->next=e;
			e->next->previous=e;
			e->data=data;
			l->current=e;
			l->count+=1;
		}
	}
}

void _list_move_iter(list_t *l,list_pos_t pos) {
	log_assert(l!=NULL);
	if (l->current==NULL) {
		// does nothing
	} else {
		if (l->current==l->first) {
			if (pos==LIST_LAST) {
				list_entry_t *e;
				e=l->first;
				l->first=e->next;
				l->first->previous=NULL;
				l->last->next=e;
				e->previous=l->last;
				e->next=NULL;
				l->last=e;
				l->current=e;
			}
		} else if (l->current==l->last) {
			if (pos==LIST_FIRST) {
				list_entry_t *e;
				e=l->last;
				l->last=e->previous;
				l->last->next=NULL;
				l->first->previous=e;
				e->next=l->first;
				e->previous=NULL;
				l->first=e;
				l->current=e;
			}
		} else {
			if (pos==LIST_LAST) {
				list_entry_t *e=l->current;
				e->previous->next=e->next;
				e->next->previous=e->previous;
				l->last->next=e;
				e->previous=l->last;
				e->next=NULL;
				l->last=e;
				l->current=e;
			} else {
				list_entry_t *e=l->current;
				e->previous->next=e->next;
				e->next->previous=e->previous;
				l->first->previous=e;
				e->next=l->first;
				e->previous=NULL;
				l->first=e;
				l->current=e;
			}
		} 
	}
}

