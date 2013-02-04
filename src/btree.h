#ifndef __GC_B_TREE_H__
#define __GC_B_TREE_H__

#include <stdio.h>
#include <semaphore.h>

// \extractdoc{gc_b_tree_h}

/*************************************************************************** \begin{doc}
\section{The b-tree box administration}

\subsection{Introduction}

The garbage collector elemental is basically a big administrative workforce. It keeps
track of all memory allocations and all references made to memory allocations. 

To keep track of the memory allocations and provide a reasonable fast way of finding 
out if a memory location is within the bounds of a memory allocation, as defined, the 
\'box\', we use a b-tree structure.

\subsection{Interface}

In this header file, we define the b-tree structure. We begin with a definition of 
the maximum number of entries in a B-tree bucket. As the B-tree algorithm prescribes, 
if the maximum entries in a bucket is reached, the entries are divided in two halves.

For definition of the external interface of this library, or, maybe better, module, we 
need to include the box type, for the inner fence boxes.

\end{doc} ******************************************************************************/

/*
 * Configuration
 */

#define EXPORT(f)		f
#define HAVE_THREADS
#define PROGRAM			"btree"
#ifdef VERSION
#undef VERSION
#endif
#define VERSION			"1.0"

// \begin{code}

typedef enum {
	BT_ENONE,
	BT_EINSERT,
	BT_EINSERTUNIQUE,
	BT_EREMOVE,
	BT_ESAVE,
	BT_ELOAD,
	BT_EVISUALIZE,
	BT_ESTATISTICS
} t_btree_error;

#define BT_BEGIN_WRITE	0 
#define BT_END_WRITE	1
#define BT_BOX_WRITE	2
#define BT_BEGIN_READ	3
#define BT_END_READ		4
#define BT_BOX_READ 	5

typedef int t_btree_io;

#define BT_OK		 0
#define BT_ERROR	-1
#define BT_NONE		 0

// flags:
#define BT_FLAGS_NONE 0
#define BT_UNIQUE	 1
#define BT_FLAGS_UNIQUE 1


// monitoring definitions
#ifndef HAVE_THREADS
#define DECLARE_MONITOR(m)
#define INIT_MONITOR(m)			0
#define DESTROY_MONITOR(m)
#define ENTER_MONITOR(m)
#define LEAVE_MONITOR(m)
#define ALLOC_MONITOR(m)		1
#else
#define DECLARE_MONITOR(m)		sem_t *m
#define ALLOC_MONITOR(m)		(m=(sem_t *) malloc(sizeof(sem_t)),m!=NULL)
#define DESTROY_MONITOR(m)		sem_destroy(m)
#define INIT_MONITOR(m)			(sem_init(m,0,1)==0)
#define ENTER_MONITOR(m)		sem_wait(m)
#define LEAVE_MONITOR(m)		sem_post(m)
#endif

typedef struct __t__btree__ {
	DECLARE_MONITOR(monitor);
	int internal;
	
	void *(*malloc)(void *p,void **padr,size_t size);
	void (*free)(void *p);
	void (*pset)(void **p0,void *p1);				
	void (*finalizer)(void *box);
	
	int  (*cmp)(struct __t__btree__ *H,void *box1,void *box2);
	int  (*in)(struct __t__btree__ *H,void *pointer,void *box);
	
	int  (*writer)(struct __t__btree__ *H,t_btree_io behaviour,void *box,void *out,int elements);
	int  (*reader)(struct __t__btree__ *H,t_btree_io behaviour,void **box,void *in,int *elements);
	
	char 			*errmsg;
	t_btree_error 	 err;
	
	int				 unique;
	
	void *reservedForCpp;
	
	void *freelist;
	void *top;
} t_btree;
// \end{code}

/*************************************************************************** \begin{doc}
Now, at this moment, we only define the interface to the outside world. The
whole library is to be thread safe. All these issues are handled in the inner
'c' part.
\end{doc} ******************************************************************************/

// \begin{code}

EXPORT(t_btree 	*bt_new				(int flags,int (*cmp)(t_btree *H,void * box1,void * box2)));
EXPORT(t_btree  *bt_new_ualloc		(int flags,
									 int (*cmp)(t_btree *H,void * box1,void * box2),
									 void *(*malloc)(void *p,void **padr,size_t size),
									 void (*free)(void *p),
			 					     void (*pset)(void **p0,void *p1),
									 void (*finalize)(void *box)
									));
EXPORT(int       bt_destroy 		(t_btree *tree));

EXPORT(void 	 bt_set_in			(t_btree *tree,int (*in)(t_btree *H,void * pointer, void * box)));
EXPORT(void 	 bt_set_cmp			(t_btree *tree,int (*cmp)(t_btree *H,void * box1,void * box2)));
EXPORT(void		 bt_set_writer		(t_btree *tree,int  (*writer)(t_btree *H,t_btree_io behaviour,void * box,void *out,int elements)));
EXPORT(void		 bt_set_reader		(t_btree *tree,int  (*reader)(t_btree *H,t_btree_io behaviour,void **box,void *in,int *elements)));
EXPORT(void		 bt_set_finalizer   (t_btree *tree,void (*finalize)(void *box)));

EXPORT(int		 bt_insert			(t_btree *tree,void * box));
EXPORT(int		 bt_remove			(t_btree *tree,void * box));

EXPORT(void		*bt_get				(t_btree *tree,void *pointer));
EXPORT(void		*bt_min				(t_btree *tree));
EXPORT(void		*bt_max				(t_btree *tree));

EXPORT(int		 bt_map				(t_btree *tree,int (*f)(t_btree *H,void * box,void *parameters),void *parameters));

EXPORT(int		 bt_enter_monitor	(t_btree *tree));
EXPORT(void		 bt_leave_monitor	(t_btree *tree,int enter_handle));


EXPORT(int		 bt_statistics  	(t_btree *tree, FILE *out));
EXPORT(int		 bt_visualize   	(t_btree *tree, const char *filename));
EXPORT(int		 bt_boxcount		(t_btree *tree));

EXPORT(int 		 bt_save			(t_btree *tree, void *out));
EXPORT(int		 bt_load			(t_btree *tree, void *in));

EXPORT(void		 		bt_error	(t_btree *tree, t_btree_error e,const char *errormsg));
EXPORT(const char  	   *bt_errmsg	(t_btree *tree));
EXPORT(t_btree_error	bt_errno	(t_btree *tree));

EXPORT(unsigned int bt_elements		(t_btree *tree));

EXPORT(void		bt_version			(FILE *out));

// \end{code}



#endif
