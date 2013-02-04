// \extractdoc{b_tree_c}

#define DEBUG 0

/*************************************************************************** \begin{doc}
\subsection{Implementation}

We start the implementation by including some include files that we need.
Notably, we need 'config.h', that's providing the OS dependent stuff. We
need the 'threads' interface from 'config.h' (see program.info). 

\end{doc} ******************************************************************************/

// \begin{code}
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "btree.h"
#include "log.h"

#ifdef HAVE_GD
#define VISUALIZE 1
#else
#define VISUALIZE 0
#endif

// \end{code}

/*************************************************************************** \begin{doc}
Next, let's start with some definitons. Our B-Tree consists of buckets that
are interlinked. The number of entries that a bucket can hold is defined
below. A number of entries of 11 max, gives good search results. Level depth at
500.000 entries is 7, this means: max search length = 77. For a max number of
entries in a bucket of 101, the max depth = 4, this means: max search length = 404.
\end{doc} ******************************************************************************/

// \begin{code}
#define BT_ENTRIES				101		/* 0 1 2 3 4 5 6 7 8 9 10 */
#define BT_LEFT					50		/* 0 1 2 3 4              */
#define BT_MIDDLE				50		/*           5            */
#define BT_RIGHT				51		/*             6 7 8 9 10 */
#define BT_HALVE            	50		
#define BT_UNDERFLOW			50		/* underflow if used < BT_UNDERFLOW */
// #define BT_ENTRIES				5		/* 0 1 2 3 4 5 6 7 8 9 10 */
// #define BT_LEFT					2		/* 0 1 2 3 4              */
// #define BT_MIDDLE				2		/*           5            */
// #define BT_RIGHT				3		/*             6 7 8 9 10 */
// #define BT_HALVE            	2		
// #define BT_UNDERFLOW			2		/* underflow if used < BT_UNDERFLOW */
// \end{code}


#define pa(H,a,b)		H->pset((void **) &(a),(void *) (b))
#define new_bucket(H,T) if (H->freelist) { pa(H,T,(t_b_tree *) H->freelist);pa(H,H->freelist,((t_b_tree *) H->freelist)->next); } \
					 	else { T=(t_b_tree *) H->malloc((void *) T,(void **) &T,sizeof(t_b_tree)); } \
					  	((t_b_tree *) T)->used=0;((t_b_tree *) T)->nextlevel[0]=NULL; 
#define del_bucket(H,T) pa(H,((t_b_tree *) T)->next,H->freelist);pa(H,H->freelist,T);
#define del_freelist(H)	{ t_b_tree *T; pa(H,T,H->freelist);while (T) { t_b_tree *N; pa(H,N,T->next);H->free(T);pa(H,T,N); } pa(H,H->freelist,NULL); }
#define bt_clrerr(H)	{ H->errmsg=NULL;H->err=BT_ENONE; }

/*************************************************************************** \begin{doc}
A B-tree bucket itself contains pointers to the next level and entries to be compared.
The used variable contains the number of boxes used. The nextlevel pointers point are
divided as follows:

\begin{verbatim}
  * b[0] * b[1] * b[2] * b[3] ... * b[used-1] *
\end{verbatim}

Each star is a pointer to the next level. The first star points to all buckets with boxes
smaller in value than b[0]. The next star points to all buckets between b[0] and b[1], etc.

There's an assumption about the boxes and that is, that the addresses of the boxes, i.e.,
pointer address until pointer address+size of allocation, are all distinct. That is:

\assume{all boxes are unique.}

This is not a hell of an assumption. It should be logical. 
The insertion algorithm takes care that boxes are not inserted more than ones. 

\end{doc} ******************************************************************************/

// \begin{code}
typedef struct _s_b_tree_ {
	struct _s_b_tree_ *next;
	int                used;
	struct _s_b_tree_ *nextlevel[BT_ENTRIES+1];
	void              *box      [BT_ENTRIES];
} t_b_tree;
// \end{code}


/*************************************************************************** \begin{doc}
To support easy allocation of buckets, we define 'new\_bucket' and 'del\_bucket'.
\end{doc} ******************************************************************************/

// \begin{code}

// \end{code}				



/*************************************************************************** \begin{doc}
To compare two box entries we define an inline 'functions' to compare two
memory allocations. %There's an assumption here, the assumption is, following:

%\assume{sizeof(void *) equals sizeof(int)}

Note: For bt\_diff(b1,b2) to be positive, condition bt\_gt(b2,b1) should be true.

\end{doc} ******************************************************************************/

#define bt_lt(H,b1,b2)	(H->cmp(H,b1,b2)<0)
#define bt_gt(H,b1,b2)	(H->cmp(H,b1,b2)>0)
#define bt_le(H,b1,b2)	(H->cmp(H,b1,b2)<=0)
#define bt_ge(H,b1,b2)	(H->cmp(H,b1,b2)>=0)
#define bt_eq(H,b1,b2)	(H->cmp(H,b1,b2)==0)
#define bt_ne(H,b1,b2)	(H->cmp(H,b1,b2)!=0)
#define bt_in(H,p,b)	(H->in(H,p,b))

static void *_malloc(void *p, void **padr, size_t s)
{
	return malloc(s);
}

static void _free(void *p)
{
	free(p);
}

static void _pset(void **p0,void *p1)
{
	*p0=p1;
}

EXPORT(t_btree  *bt_new_ualloc		(int flags,
									 int (*_cmp)(t_btree *H,void * box1,void * box2),
									 void *(*_malloc)(void *p,void **padr,size_t size),
									 void (*_free)(void *p),
			 					     void (*_pset)(void **p0,void *p1),
									 void (*_finalize)(void *box)
									))
{
	t_btree *H=(t_btree *) _malloc(H,(void **) &H,sizeof(t_btree));
	
	H->unique=flags & BT_UNIQUE;
	
	if (!H) {
		log_fatal("Cannot allocate t_btree");
		exit(1);
	}
	
	H->cmp=_cmp;
	H->in=NULL;
	H->malloc=_malloc;
	H->free=_free;
	H->pset=_pset;
	H->finalizer=_finalize;
	
	H->writer=NULL;
	H->reader=NULL;
	
	H->errmsg=NULL;
	H->err=BT_ENONE;
	
	H->top=NULL;
	H->freelist=NULL;
	if (!ALLOC_MONITOR(H->monitor)) { log_error("Cannot allocate monitor!"); }
	if (!INIT_MONITOR(H->monitor)) { log_error("Cannot initialize monitor!"); }
	H->internal=(1==0);

return (t_btree *) H;
}


EXPORT(t_btree *bt_new(int flags,int (*cmp)(t_btree *H,void *box1,void *box2)))
{
return bt_new_ualloc(flags,cmp,_malloc,_free,_pset,NULL);
}

EXPORT(void bt_set_in(t_btree *tree,int (*in)(t_btree *H,void *pointer, void *box)))
{
	if (!tree->internal) { ENTER_MONITOR(tree->monitor); }
	tree->in=in;
	if (!tree->internal) { LEAVE_MONITOR(tree->monitor); }
}


EXPORT(void bt_set_cmp(t_btree *tree,int (*cmp)(t_btree *H, void *box1,void *box2)))
{
	if (!tree->internal) { ENTER_MONITOR(tree->monitor); }
	tree->cmp=cmp;
	if (!tree->internal) { LEAVE_MONITOR(tree->monitor); }
}

EXPORT(void bt_set_writer(t_btree *tree,int (*writer)(t_btree *H,t_btree_io behaviour, void *box,void *out,int elements)))
{
	if (!tree->internal) { ENTER_MONITOR(tree->monitor); }
	tree->writer=writer;
	if (!tree->internal) { LEAVE_MONITOR(tree->monitor); }
}

EXPORT(void bt_set_reader(t_btree *tree,int (*reader)(t_btree *H,t_btree_io behaviour, void **box,void *out,int *elements)))
{
	if (!tree->internal) { ENTER_MONITOR(tree->monitor); }
	tree->reader=reader;
	if (!tree->internal) { LEAVE_MONITOR(tree->monitor); }
}

EXPORT(void		 bt_set_finalizer   (t_btree *tree,void (*finalize)(void *box)))
{
	if (!tree->internal) { ENTER_MONITOR(tree->monitor); }
	tree->finalizer=finalize;
	if (!tree->internal) { LEAVE_MONITOR(tree->monitor); }
}


EXPORT(int bt_destroy(t_btree *tree))
{
int result;	

	if (!tree->internal) { ENTER_MONITOR(tree->monitor); }
	
	{
		t_btree _T=*tree,*T=&_T;
		int i=T->internal;
		
		T->internal=(1==1);
		
		if (T->finalizer) {
			while(T->top) {
				bt_remove(T,bt_min(T));
			}
		}
	
		if (T->top) { 
			result=BT_ERROR; 
		}
		else { 
			result=BT_OK; 
		}
		
		T->internal=i;
		*tree=_T;
	}
	
	if (!tree->internal) { LEAVE_MONITOR(tree->monitor); }
	
	if (result!=BT_ERROR) {
		DESTROY_MONITOR(tree->monitor);
		
		del_freelist(tree);
		tree->free(tree);
	}
	
return result;	
}

int bt_enter_monitor(t_btree *T) {
	log_fcall_enter();
	ENTER_MONITOR(T->monitor);
	int i=T->internal;
	T->internal=(1==1);
	log_return(i);
}

void bt_leave_monitor(t_btree *T,int handle) {
	log_fcall_enter();
	T->internal=handle;
	LEAVE_MONITOR(T->monitor);
}

/*************************************************************************** \begin{doc}
Let's implement the functions that we need. First the insert function.

\subsubsection{B-Tree Insert}

The insert function inserts elements in B-tree buckets. 

\begin{itemize}
\dash We begin with defining the possibility to insert a box item with it's left and
      right pointers; left being all the values <box and right representing all values
      >box.
\dash The algorithm is called with 'left' and 'right' being NULL and 'box' being the
      to be inserted value.
\end{itemize}
\doc2code*/

typedef struct {
	void   	 *box;
	t_b_tree *left;
	t_b_tree *right;
} bt_box;

/*\code2doc
\begin{itemize}
\dash If 'box' already exists in the b-tree, it isn't inserted again.
\dash The insertion of a box element in a bucket that's not full is trivial. 
      The algoritm always inserts a new box along with it's pointers. 
      Afterwards it is checked if the bucket is full. 
\dash If a bucket is full, it needs to be split in two halves. The box in the middle
      needs to 'bubble' up, along with the pointers to the two new buckets. So we
      need to arrange for this possibility. To let this algoritm work, the number of 
      entries (boxes) in a bucket needs to be odd.
\dash The box that 'bubbles up' should be inserted into the current box along with
      the pointers. If this box is full, the same exercise is repeated. 
\dash Note, that the left pointer is always the current bucket. The right pointer
      points to a new bucket.      
\dash Except on the top level. There, a new bucket is made, inserting only one entry
      with the two pointers.          
\end{itemize}      


In order to let the middle 'box' bubbleup, along with the left and right pointers to 
the next level


\end{doc} ******************************************************************************/
// \begin{code}

#if (DEBUG==1)
static void _bt_contents(t_btree *H);
#endif

static bt_box *_b_tree_insert(t_btree *H,bt_box *box,t_b_tree *t);

int bt_insert(t_btree *H,void *box)
{
	bt_box b,*B;
	
/*\code2doc
		First, a inserton box b is made and initialized
\doc2code*/
	
	b.box=box;
	b.left=NULL;
	b.right=NULL;
	
/*\code2doc
		Then, we enter the monitor section, to ensure this
		operation will be atomic.
\doc2code*/	

#if (DEBUG==1)
	_bt_contents(H);
#endif 

	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
/*\begin{skip}*/	
#if (DEBUG==1)	
	log_debug2("inserting box %d",*(int *) box);
#endif	
/*\end{skip}*/

/*\code2doc
	Next, insertion can begin.
\doc2code*/	
	{
		t_btree _T=*H,*T=&_T;
		int i=T->internal;

		T->internal=i;
		bt_clrerr(T);

		if (!T->top) { new_bucket(T,T->top); }
		B=_b_tree_insert(T,&b,T->top);
		
/*\code2doc		
		if B is not NULL, the previous top bucket was full 
		This means, we're making a new top bucket 
\doc2code */		
		
		if (B) {
			new_bucket(T,T->top);
			pa(T,((t_b_tree *) T->top)->box[0],B->box);
			pa(T,((t_b_tree *) T->top)->nextlevel[0],B->left);
			pa(T,((t_b_tree *) T->top)->nextlevel[1],B->right);
			((t_b_tree *) T->top)->used=1;

/*\begin{skip}*/	
#if (DEBUG==1)	
	log_debug("new top level made\n");
#endif		
/*\end{skip}*/
		}
		
		T->internal=i;
		*H=_T;
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
	
return (H->err!=BT_ENONE) ? BT_ERROR : BT_OK;
}

/*\code2doc
The static function \_gc\_b\_tree\_insert() is used to insert the
box into the b-tree.
\doc2code*/

bt_box *_b_tree_insert(t_btree *H,bt_box *box,t_b_tree *t)
{
int i,k;	
	
/*\code2doc
	 First, find a  position that can be used in the current bucket.
\doc2code*/
	
	for(i=0;i<t->used && bt_gt(H,box->box,t->box[i]);i++);
	
/* \\code2doc
	Check for equalness, if equal, return NULL right away, the
	box already has been inserted 
\doc2code*/

#if (DEBUG==1)
	log_debug2("Found i=%d",i);
#endif
	   
	if (H->unique && i<t->used && bt_eq(H,box->box,t->box[i])) {
		bt_error(H,BT_EINSERTUNIQUE,"Insertion of duplicate elements is not allowed in this btree");
		return NULL;
	}
	
/*\code2doc
	Next, check if there's a next level. If so, this is the
	trivial search case; we need to enter the next level of
	the b-tree.
\doc2code*/	
	
	if (t->nextlevel[i]) {
		pa(H,box,_b_tree_insert(H,box,t->nextlevel[i]));
	}

/*\code2doc
	If the box element has been inserted at a lower level,
	and there's no overflow of the bucket, the returned box
	element will be NULL. No insertion is needed further.
	However, if the box isn't NULL, an overflow occured at
	lower levels and the lower bucket has been split up in two.
	The middle box element bubbles 	up from lower levels and
	needs to be inserted in the current bucket, at the place,
	already searched, i.e. i.
\doc2code*/		

/*\begin{skip}*/
#if (DEBUG==1)		
	{int j;
	    log_debug2("used=%d",t->used);
		for(j=0;j<t->used;j++) { log_debug3("b[%d]=%8d, ",j,*(int *) t->box[j]); }
		for(j=0;j<=t->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[j]); }
    }
#endif
/*\end{skip}*/
	
	if (box) {
		t->used+=1;
		for(k=t->used-1;k>i;k--) { pa(H,t->box[k],t->box[k-1]); }
		for(k=t->used;k>i;k--) { pa(H,t->nextlevel[k],t->nextlevel[k-1]); }
		pa(H,t->box[i],box->box);
		pa(H,t->nextlevel[i],box->left);
		pa(H,t->nextlevel[i+1],box->right);
	}

/*\begin{skip}*/
#if (DEBUG==1)		
	{int j;
	    log_debug2("used=%d",t->used);
		for(j=0;j<t->used;j++) { log_debug3("b[%d]=%8d, ",j,*(int *) t->box[j]); }
		for(j=0;j<=t->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[j]); }
    }
#endif
/*\end{skip}*/
	
/*\code2doc
	Afer insertion of the box in the bucket, the bucket may be full.
	If so, there's an overflow situation. The bucket will be split into
	two halves and the middle box will bubble up to the level above
	the current one, by returning it to the recursion.
	
	If the bucket wasn't full, NULL is returned, to indicate that no 
	further action is required.
\doc2code*/		

	if (t->used==BT_ENTRIES) {
		/* Ok, start dividing this thing and bubble up the results */
		
		t_b_tree *left,*right;
		
		pa(H,left,t);
		left->used=BT_HALVE;
		
		new_bucket(H,right);
		right->used=BT_HALVE;
		for(k=0,i=BT_RIGHT;i<BT_ENTRIES;i++,k++) {
			pa(H,right->box[k],t->box[i]);
			pa(H,right->nextlevel[k],t->nextlevel[i]);
		}
		pa(H,right->nextlevel[k],t->nextlevel[i]);
		
		pa(H,box->box,t->box[BT_MIDDLE]);
		pa(H,box->left,left);
		pa(H,box->right,right);
		
		return box;
	}
	else {
		/* It wasn't full, no problem at all, return NULL to indicate */
		/* That no further action is required */
		
		return NULL;
	}
	
}
// \end{code}


/*************************************************************************** \begin{doc}
\subsubsection{B-Tree Get}

To get a box from the b-tree, function gc\_bt\_get() is used. gc\_bt\_get() searches
an element in the tree and returns it, if it is in there. If not, it will return 
NULL.

To be complete, gc\_bt\_get() will search for a void pointer to be in the memory
allocated range. It expects 'pointer' to be in the range of a box, i.e., for all
boxes, it checks if box<=pointer<(box+box->allocated\_size)'.

It will return the box where the pointer resides in.
\end{doc} ******************************************************************************/

// \begin{code}
static void *_b_tree_get(t_btree *H,void *pointer,t_b_tree *t);

void *bt_get(t_btree *H,void *pointer)
{
	void *box;

	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{
		t_btree _T=*H,*T=&_T;
		int i=T->internal;
		T->internal=(1==1);
		
		if (T->top) { box=_b_tree_get(T,pointer,T->top); }
		else { box=NULL; }
		
		T->internal=i;
		*H=_T;
	}	
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }

	return box;
}

void *_b_tree_get(t_btree *H,void *pointer,t_b_tree *t)
{
int i;
int in;
/*\code2doc
	  Because we're working with ranges, and the pointer will always be withing box and box+box->allocated\_size
	  bytes, and the order of the buckets is ascending, we need to search from the upper pointers to the
	  lower pointers. Otherwise, we'll miss our target box.
\doc2code*/	  
/*\begin{skip}*/

#if (DEBUG==1)		
	{int j;
	    log_debug2("searching %d, t=%d",*(int *) pointer,t);
		for(j=0;j<t->used;j++) { log_debug3("b[%d]=%8d, ",j,*(int *) t->box[j]); }
		for(j=0;j<=t->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[j]); }
    }
#endif
/*\end{skip}*/

	for(i=t->used-1;i>=0 && bt_lt(H,pointer,t->box[i]);i--);
	
#if (DEBUG==1)
	log_debug2("found i=%d",i);
#endif
	
	if (i>=0) {
		if (H->in) { in=bt_in(H,pointer,t->box[i]); }
		else { in=bt_eq(H,pointer,t->box[i]); }
	}
	
	if (i>=0 && in) {
/*\begin{skip}*/
#if (DEBUG==1)		
		log_debug3("%d in %d  found",pointer,t->box[i]);
#endif		
/*\end{skip}*/
		return t->box[i];
	}
	else if (t->nextlevel[i+1]) {
		return _b_tree_get(H,pointer,t->nextlevel[i+1]);
	}
	else {
		return NULL;
	}
}
// \end{code}


static void *_bt_min(t_b_tree *T);
static void *_bt_max(t_b_tree *T);

void *bt_min(t_btree *H)
{
void *box;	
	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{ 
		if (!H->top) { box=NULL; }
		else {
			box=_bt_min(H->top);
		}
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
return box;	
}

void *bt_max(t_btree *H)
{
void *box;
	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{ 
		if (!H->top) { box=NULL; }
		else {
			box=_bt_max(H->top);
		}
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
return box;	
}


void *_bt_min(t_b_tree *t)
{
 	if (t->nextlevel[0]) { return _bt_min(t->nextlevel[0]); }
 	else { return t->box[0]; }
}

void *_bt_max(t_b_tree *t)
{
	if (t->nextlevel[t->used]) { return _bt_max(t->nextlevel[t->used]); }
	else { return t->box[t->used-1]; }
}


static unsigned int _bt_elements(t_b_tree *t);

unsigned int bt_elements(t_btree *H)
{
unsigned int r;
	if (H->top) { r=_bt_elements((t_b_tree *) H->top); }
	else { r=0; }
return r;		
}

unsigned int _bt_elements(t_b_tree *t)
{
int i;
int c=t->used;
	for(i=0;i<t->used;i++) {
		if (t->nextlevel[i]) { c+=_bt_elements(t->nextlevel[i]); }
	}
	if (t->nextlevel[i]) { c+=_bt_elements(t->nextlevel[i]); }
return c;
}

typedef struct {
	int   	 elements;
	int  	 behaviour;
	void 	*out;
} t_internal_bt_save;

static int bt_save_writer(t_btree *H,void *box,t_internal_bt_save *out);

int bt_save(t_btree *H,void *out)
{
int retval=BT_OK;

	if (!H->internal) { ENTER_MONITOR(H->monitor); }
		
	{t_btree _T=*H,*T=&_T;
	 int 				elements;
	 t_internal_bt_save info;
	 	int i=T->internal;

		T->internal=(1==1);
		
		elements=bt_elements(T);
		info.elements=elements;
		info.out=out;
		info.behaviour=BT_BOX_WRITE;
		
		if (T->writer) {
			T->writer(T,BT_BEGIN_WRITE,NULL,out,elements);
			bt_map(T,(int (*)(t_btree *,void *,void *)) bt_save_writer,(void *) &info);
			T->writer(T,BT_END_WRITE,NULL,out,elements);
		}
		else {
			bt_error(T,BT_ESAVE,"No writer function, tree not written");
			retval=BT_ERROR;
		}
		
		T->internal=i;
		*H=_T;
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }

return retval;	
}

int bt_save_writer(t_btree *H,void *box,t_internal_bt_save *out)
{
	return H->writer(H,out->behaviour,box,out->out,out->elements);
}


int bt_load(t_btree *H,void *in)
{
unsigned int elements,i;
void  *box;
int retval=BT_OK;

	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{t_btree _T=*H,*T=&_T;
		int i=T->internal;
		T->internal=(1==1);
		
		retval=H->reader(H,BT_BEGIN_READ,&box,in,&elements);
		
		if (!retval) {
			for(i=0;i<elements;i++) {
				if (H->reader(H,BT_BOX_READ,&box,in,&elements)==BT_ERROR) {
					i=elements;
					//errormsg
				}
				else {
					if (bt_insert(T,box)==BT_ERROR) {
						i=elements;
						//errormsg has been done by bt_insert
					}
				}
			}
			if (i==elements && box==NULL) {
				H->errmsg="Cannot read btree entirely, file and memory may be corrupted";
				retval=BT_ERROR;
			}
		}			
		
		if (!retval) {
			retval=H->reader(H,BT_END_READ,&box,in,&elements);
		}
		
		T->internal=i;
		*H=_T;
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }

return retval;	
}

int bt_apply(t_btree *T,void *pointer,int (*f)(void *box,void *parameters),void *parameters) {
	int retval=-1;
	if (!T->internal) { ENTER_MONITOR(T->monitor); }
	{
		int i=T->internal;
		T->internal=(1==1);
		void *box=bt_get(T,pointer);
		if (box!=NULL) {
			retval=f(box,parameters);
		}
		T->internal=i;
	}
	if (!T->internal) { LEAVE_MONITOR(T->monitor); }
	return retval;
}

static int _bt_map(t_btree *T,t_b_tree *t,int (*f)(t_btree *T,void *box,void *parameters),void *parameters);

int bt_map(t_btree *H,int (*f)(t_btree *H,void *box,void *parameters),void *parameters)
{
int retval;	
	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{t_btree _T=*H,*T=&_T;
	 int i=T->internal;
		T->internal=(1==1);
		
		if (T->top) { retval=_bt_map(T,T->top,f,parameters); }
		
		T->internal=i;
		*H=_T;
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
return retval;	
}

int _bt_map(t_btree *T,t_b_tree *t,int (*f)(t_btree *T,void *box,void *parameters),void *parameters)
{
int i;
int retval=BT_OK;
	for(i=0;i<t->used && retval==BT_OK;i++) {
		if (t->nextlevel[i]) { retval=_bt_map(T,t->nextlevel[i],f,parameters); }
		retval=f(T,t->box[i],parameters);
	}
	if (t->nextlevel[i]) { retval=_bt_map(T,t->nextlevel[i],f,parameters); }
return retval;	
}

/*************************************************************************** \begin{doc}
\subsubsection{B-Tree Remove}

To remove a box from the b-tree, function gc\_bt\_remove is used. This function
searches the b-tree for the box and removes it from the bucket. 

A b-tree can never be unbalanced. This means, that, if an element is not in a leaf,
all elements in the node will have a pointer to a next node with elements smaller and
to a next node with elements bigger than the current element value.

The algorithm is as follows:

\begin{itemize}
\dash 
\end{itemize}
\end{doc} ******************************************************************************/

//\begin{code}

typedef struct {
	void 	 *box;
	t_b_tree *branch;
	int       i;
} rm_box;

static void _b_tree_rm(t_btree *H,void *box,t_b_tree *t,void **found, int state, int underflow);

#define RM_SEARCH 0
#define RM_END	  1

int bt_remove(t_btree *H,void *box)
{

	if (!H->internal) { ENTER_MONITOR(H->monitor); }

	{	
		t_btree _T=*H,*T=&_T;
		int i=T->internal;
		T->internal=(1==1);
		bt_clrerr(T);
		
		if (T->top) {
			_b_tree_rm(T,box,T->top,NULL,RM_SEARCH,BT_UNDERFLOW);
			if (((t_b_tree *) T->top)->used==0) {
				t_b_tree *t;
				pa(H,t,((t_b_tree *) T->top)->nextlevel[0]);
				del_bucket(T,T->top);
				pa(H,T->top,t);
			}
		}
		
//\begin{skip}
#if (DEBUG==1)	
		log_debug2("remove, top=%d",T->top);
#endif	
//\end{skip}


		T->internal=i;
		*H=_T;
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
	
return (H->err!=BT_ENONE) ? BT_ERROR : BT_OK;
}


#define leaf(t,i)  (!(t->nextlevel[i] || t->nextlevel[i+1]))

static void _b_tree_rm(t_btree *H,void *box,t_b_tree *t,void **found,int state,int underflow)
{
int i;	

	// Not to be found at all
    
    if (!t) { return; }

/*\begin{skip}*/
#if (DEBUG==1)		
	{int j;
	    log_debug("rm: searching:");
	    char buf[2048]
	    sprintf(buf,"box=%d, t=%d, used=%d, i=%d, state=%d, underflow=%d, found=%d",*(int *) box,t,t->used,i,state,underflow,found);
	    log_debug(buf);
		for(j=0;j<t->used;j++) { log_debug3("b[%d]=%8d, ",j,t->box[j]); }
		for(j=0;j<=t->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[j]); }
    }
#endif
/*\end{skip}*/

    // search box
    
    if (state==RM_SEARCH) {
    	for(i=0;i<t->used && bt_gt(H,box,t->box[i]);i++);
    	if (!(i<t->used && bt_eq(H,box,t->box[i]))) {
	    	
	    	// Not found
	    	
	    	if (i!=t->used && bt_gt(H,box,t->box[i])) { i+=1; }
	    	_b_tree_rm(H,box,t->nextlevel[i],found,RM_SEARCH,underflow);
    	}	
    	else {
	    	if (!leaf(t,i)) {
		    	_b_tree_rm(H,box,t->nextlevel[i+1],&t->box[i],RM_END,underflow);
	    	}
	    	else { // We're in a leaf, we can delete this thing right away.
	    	    int k;
	    	    for(k=i;k<t->used;k++) {
		    	    pa(H,t->box[k],t->box[k+1]);
		    	    pa(H,t->nextlevel[k],t->nextlevel[k+1]);
	    	    }
	    	    t->used-=1;
	    	    
	    	    // And we return right away from this level in the tree
	    	    
	    	    return;
	    	}
    	}
	}
	else if (state==RM_END) {
		i=0;
		if (t->nextlevel[i]) {
			_b_tree_rm(H,box,t->nextlevel[i],found,RM_END,underflow);
		}
		else {
			int k;
			
			pa(H,*found,t->box[i]);
			for(k=i;k<t->used;k++) {
				pa(H,t->box[k],t->box[k+1]);
				pa(H,t->nextlevel[k],t->nextlevel[k+1]);
			}
			t->used-=1;
			
	    	// And we return right away from this level in the tree
	    	    
	    	return;
		}
	}
	
	// OK, Now, a subtree has been altered, and we're a level above 
	// it. Let's look if we have to do something. Is there an underflow
	// situation?
	
	// Are we at the end of this bucket?
	
	if (i==t->used) { i-=1; }
	
	// Is this bucket a leaf?
	
	if (leaf(t,i)) { return; }
	
/*\begin{skip}*/
#if (DEBUG==1)		
	{int j;
	    log_debug("rm: before operations:\n");
	    char buf[2048]
	    sprintf(buf,"box=%d, t=%d, used=%d, i=%d, state=%d, underflow=%d, found=%d\n",box,t,t->used,i,state,underflow,found);
	    log_debug(buf);
		for(j=0;j<t->used;j++) { log_debug3("b[%d]=%8d, ",j,t->box[j]); }
		for(j=0;j<=t->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[j]); }
	    
	    if (t->nextlevel[i]) {
		    log_debug3("left: l=%d, l->used=%d",t->nextlevel[i],t->nextlevel[i]->used);
	    	for(j=0;j<t->nextlevel[i]->used;j++) { log_debug3("b[%d]=%8d, ",j,t->nextlevel[i]->box[j]); }
	    	for(j=0;j<t->nextlevel[i]->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[i]->nextlevel[j]); }
    	}
    	
	    if (t->nextlevel[i+1]) {
		    log_debug3("right: r=%d, r->used=%d\n",t->nextlevel[i+1],t->nextlevel[i+1]->used);
	    	for(j=0;j<t->nextlevel[i+1]->used;j++) { log_debug3("b[%d]=%8d, ",j,t->nextlevel[i+1]->box[j]); }
	    	for(j=0;j<t->nextlevel[i+1]->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[i+1]->nextlevel[j]); }
    	}
    }
#endif
/*\end{skip}*/

	// Normal case
	{t_b_tree *l,*r;
	 int uLeft,uRight;
	 
	  pa(H,l,t->nextlevel[i]);
	  pa(H,r,t->nextlevel[i+1]);
	  uLeft=l->used;
	  uRight=r->used;
	
	  // underflow of the left?
	  
	  if (uLeft<underflow) { 
		  // hmm, can we distribute from the right?
		  if (uRight>underflow) {
#if (DEBUG==1)			  
			  log_debug("redistributing from right to left");
#endif			  
			  // we need to distribute from right to left.
			  
			  // How many keys can we move?
			  
			  int number=(uRight-uLeft)>>1;
			  int k=0,j;
			  
			  while(number>0) {
				  // Rotate from right to left
				  
				  pa(H,l->box[l->used],t->box[i]);
				  pa(H,l->nextlevel[l->used+1],r->nextlevel[k]);
				  pa(H,t->box[i],r->box[k]);
				  k+=1;
				  number-=1;
				  l->used+=1;
			  }
			  
			  // copy r->k part, repair r.
			  
			  for(j=0;k<r->used;k++,j++) {
				  pa(H,r->box[j],r->box[k]);
				  pa(H,r->nextlevel[j],r->nextlevel[k]);
			  }
			  pa(H,r->nextlevel[j],r->nextlevel[k]);
			  r->used=j;
		  }
	  }
	  else if (uRight<underflow) {
		  // hmm, can we distribute from the left?
		  if (uLeft>underflow) {
			  // we need to distribute from left to right.
#if (DEBUG==1)			  
			  log_debug("redistributing from left to right");
#endif			  
			  
			  // How many keys can we move?
			  
			  int number=(uLeft-uRight)>>1;
			  int k;
			  
			  // Make place in r.
			  
			  r->used+=number;
			  pa(H,r->nextlevel[r->used],r->nextlevel[r->used-number]);
			  for(k=r->used-1;k>=number;k--) {
				  pa(H,r->box[k],r->box[k-number]);
				  pa(H,r->nextlevel[k],r->nextlevel[k-number]);
			  }
			  
			  while(number>0) {
				  // rotate from left to right
				  
				  pa(H,r->box[k],t->box[i]);
				  pa(H,r->nextlevel[k],l->nextlevel[l->used]);
				  pa(H,t->box[i],l->box[l->used-1]);
				  
				  k-=1;
				  l->used-=1;
				  number-=1;
		  	  }
		  	  
		  	  // This should be ok now.
		  }
	  }
	
	  // maybe now, there is a complete underflow situation
	  
	  if ((uLeft+uRight)<(underflow<<1)) {
		  // combine
		  t_b_tree *m;
		  pa(H,m,l);

#if (DEBUG==1) 		  		  
		  log_debug("merging");
#endif		  
		  
		  int k,j;
		  
		  k=m->used;
		  pa(H,m->box[k],t->box[i]); // if nextlevel[k] exists, it's the right one for box[i]
		  k+=1;
		  
		  for(j=0;j<r->used;j++,k++) {
			  pa(H,m->box[k],r->box[j]);
			  pa(H,m->nextlevel[k],r->nextlevel[j]);
		  }
		  pa(H,m->nextlevel[k],r->nextlevel[j]);  // and the last one.
		  m->used=k;
		  
		  // Now, delete box[i].
		  
		  pa(H,t->nextlevel[i+1],l);
		  for(j=i;j<t->used;j++) {
			  pa(H,t->box[j],t->box[j+1]);
			  pa(H,t->nextlevel[j],t->nextlevel[j+1]);
		  }
		  pa(H,t->nextlevel[j],NULL);
		  t->used-=1;
		  
		  // delete r.
		  
		  del_bucket(H,r);
	  }
  	}
  	
/*\begin{skip}*/
#if (DEBUG==1)		
	if (i>t->used) { i-=1; }
	
	{int j;
	    log_debug("rm: done all operations:");
	    char buf[2048];
	    sprintf(buf,"box=%d, t=%d, used=%d, i=%d, state=%d, underflow=%d, found=%d\n",box,t,t->used,i,state,underflow,found);
	    log_debug(buf);
		for(j=0;j<t->used;j++) { log_debug3("b[%d]=%8d, ",j,t->box[j]); }
		for(j=0;j<=t->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[j]); }
	    
	    if (t->nextlevel[i]) {
		    log_debug3("left: l=%d, l->used=%d",t->nextlevel[i],t->nextlevel[i]->used);
	    	for(j=0;j<t->nextlevel[i]->used;j++) { log_debug3("b[%d]=%8d, ",j,t->nextlevel[i]->box[j]); }
	    	for(j=0;j<t->nextlevel[i]->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[i]->nextlevel[j]); }
    	}
    	
	    if (t->nextlevel[i+1]) {
		    log_debug3("right: r=%d, r->used=%d",t->nextlevel[i+1],t->nextlevel[i+1]->used);
	    	for(j=0;j<t->nextlevel[i+1]->used;j++) { log_debug3("b[%d]=%8d, ",j,t->nextlevel[i+1]->box[j]); }
	    	for(j=0;j<t->nextlevel[i+1]->used;j++) { log_debug3("n[%d]=%8d, ",j,t->nextlevel[i+1]->nextlevel[j]); }
    	}
    }
#endif
/*\end{skip}*/
}



/*************************************************************************** \begin{doc}
\subsubsection{B-Tree Statistics}

B-tree statistics walks the B-Tree and figures out the current maximum tree depth,
the number of buckets and the number of buckets stored. 

\end{doc} ******************************************************************************/

//\begin{code}

static void _b_tree_stats(t_btree *H,t_b_tree *t,int cl,int *max,int *boxcount,int *levelscount);


int bt_boxcount(t_btree *H) {
	int maxdepth=0,boxcount=0,levelscount=0,fdepth=0;

	if (!H->internal) { ENTER_MONITOR(H->monitor); }

	{
		int i=H->internal;
		H->internal=(1==1);

		{
			if (H->top) {
				levelscount+=1;
				_b_tree_stats(H,H->top,1,&maxdepth,&boxcount,&levelscount);
			}
		}

		H->internal=i;
	}

	if (!H->internal) { LEAVE_MONITOR(H->monitor); }

	return boxcount;
}


int bt_statistics(t_btree *H,FILE *out)
{
int    maxdepth=0,boxcount=0,levelscount=0,fdepth=0;
time_t t;
int    retval=BT_OK;

	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{
		t_btree _T=*H,*T=&_T;
		int i=T->internal;
		T->internal=(1==1);
		
		time(&t);
		if (T->top) { levelscount++;_b_tree_stats(T,T->top,1,&maxdepth,&boxcount,&levelscount); }
		
		T->internal=i;
		*H=_T;
	}
	
	{t_b_tree *T=H->freelist;
		while(T) {
			T=T->next;
			fdepth+=1;
		}
	}
	
	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
	
	fprintf(out,"--------------------------------------------------\n");
	fprintf(out,"b_tree statistics at %s",ctime(&t));
	fprintf(out,"--------------------------------------------------\n");
	fprintf(out,"Maximum tree depth            : %8d\n",maxdepth);
	fprintf(out,"Number of boxes stored        : %8d\n",boxcount);
	fprintf(out,"Number of buckets             : %8d\n",levelscount);
	fprintf(out,"Freelist depth                : %8d\n",fdepth);
	fprintf(out,"--------------------------------------------------\n");
	
return retval;	
}

void _b_tree_stats(t_btree *H,t_b_tree *t,int cl, int *max,int *boxcount, int *levelscount)
{
int i;
	if (cl>*max) { *max=cl; }
	*boxcount+=t->used;
	
	for(i=0;i<=t->used;i++) {
		if (t->nextlevel[i]) {
			*levelscount+=1;
			_b_tree_stats(H,t->nextlevel[i],cl+1,max,boxcount,levelscount);
		}
	}
}


#if (VISUALIZE==1) 
#include <gd.h>

#define V_BOX 		1
#define V_BRANCH 	2

static void _gc_bt_visualize(t_btree *H,t_b_tree *t,gdImagePtr im,int *colors,int Ncolors,int X,int Y,int Z,int type);	

int bt_visualize(t_btree *H,const char *filename)
{
	
	/* Declare the image */
	gdImagePtr im;
	/* Declare output files */
	FILE *jpegout,*pngout;
	
	/* Declare color indexes */
	int N=6;
	int Ncolors=N*N*N;
	int F=255/N;
	int colors[Ncolors];
	int x,y,z;
	int i,j,k,h;
	int grey;
	
	int retval=BT_OK;
	
	x=1024;
	y=768;
	z=28;

	/* Allocate the image: 64 pixels across by 64 pixels tall */
	im = gdImageCreate(x,y);
	
	h=0;
	for(i=0;i<N;i++) {
		for(j=0;j<N;j++) {
			for(k=0;k<N;k++) {
				colors[h++]=gdImageColorAllocate(im, i*F, j*F, k*F);
			}
		}
	}
	
	grey=gdImageColorAllocate(im,255,255,255);
	gdImageFilledRectangle(im, 0, 0, x, x, grey);	
	
	if (!H->internal) { ENTER_MONITOR(H->monitor); }
	
	{
	 t_btree _T=*H,*T=&_T;
	 
	    T->internal=(1==1);
	    
		_gc_bt_visualize(H,H->top,im,colors,Ncolors,x,y,z,V_BOX);
		_gc_bt_visualize(H,H->top,im,colors,Ncolors,x,y,z,V_BRANCH);
		
		T->internal=(1==0);
		*H=_T;
	}

	if (!H->internal) { LEAVE_MONITOR(H->monitor); }
	
	/* Do the same for a JPEG-format file. */
	
	char jpg[1024],png[1024];
	sprintf(jpg,"%s.jpg",filename);
	sprintf(png,"%s.png",filename);
	
	jpegout = fopen(jpg,"wb");
	pngout  = fopen(png,"wb");

	/* Output the same image in JPEG format, using the default
		JPEG quality setting. */
	gdImageJpeg(im, jpegout, -1);
	gdImagePng(im,pngout);

	/* Close the files. */
	fclose(jpegout);
	fclose(pngout);

	/* Destroy the image in memory. */
	gdImageDestroy(im);
	
return retval;	
}

#define abs(x) 		((x<0) ? -x : x)
#define clip(x,H)	((x>H) ? H : ((x<0) ? 0 : x))

void _gc_bt_visualize(t_btree *H,t_b_tree *t,gdImagePtr im,int *colors,int Ncolors,int X,int Y,int Z,int type)
{
	int HX=X/2,HY=Y/2;
	int tx,ty,tz,x,y,z;
	int c;
	int i;
	double F,FX,FY,Fxy,Fz,f;
	
	Fxy=1;
	Fz=1;
	
	tx=((unsigned int) t)%X-HX;
	ty=((unsigned int) t)%Y-HY; 
	//tz=(Fxy*(abs(tx)+abs(ty)))/(Fz*Z);
	tz=Fz*(((abs(tx)+abs(ty))/Z)^2);
	if (tz==0) { tz=1; }

	f=4;	
	F=((double) Z)/tz;
	FX=15;
	FY=12;
	
	// project to 2d
	
	tx=((tx*FX)/tz)+HX;
	ty=((-ty*FY)/tz)+HY;
	tx=clip(tx,X);
	ty=clip(ty,Y);

	
	if (type==V_BOX) {
		for(i=0;i<t->used;i++) {
			
			x=((unsigned int) t->box[i])%X-HX;
			y=((unsigned int) t->box[i])%Y-HY; 
			//z=(Fxy*(abs(x)+abs(y)))/(Fz*Z);
			z=Fz*(((abs(x)+abs(y))/Z)^2);
			if (z==0) { z=1; }			
			
			// project to 2d

			x=((x*FX)/z)+HX;
			y=((-y*FY)/z)+HY;
			x=clip(x,X);
			y=clip(y,Y);
			
			c=((unsigned int) t->box[i])%Ncolors;
			
			gdImageFilledArc(im,x,y,2,2,0,360,colors[c],gdArc);
			gdImageLine(im,tx,ty,x,y,colors[c]);
		}
	}


	for(i=0;i<=t->used;i++) {
		if (t->nextlevel[i]) {
			_gc_bt_visualize(H,t->nextlevel[i],im,colors,Ncolors,X,Y,Z,type);
		}
	}

	if (type==V_BRANCH) {	
		for(i=0;i<=t->used;i++) {
			if (t->nextlevel[i]) {
				x=((unsigned int) t->nextlevel[i])%X-HX;
				y=((unsigned int) t->nextlevel[i])%Y-HY; 
				//z=(Fxy*(abs(x)+abs(y)))/(Fz*Z);
				z=Fz*(((abs(x)+abs(y))/Z)^2);
				if (z==0) { z=1; }			
				c=0;
				
				// project x, y, z on two dimensional grid
				
				x=((x*FX)/z)+HX;
				y=((-y*FY)/z)+HY;
				x=clip(x,X);
				y=clip(y,Y);
				
				gdImageSetThickness(im, 3);
				gdImageLine(im,tx,ty,x,y,colors[c]);
				gdImageSetThickness(im, 1);
			}
		}
		
		c=0;
		gdImageFilledArc(im, tx, ty, 4*f*F, 4*f*F, 0, 360, colors[c],gdArc);
	}
}

#else
int bt_visualize(t_btree *H,const char *filename)
{
	bt_error(H,BT_EVISUALIZE,"bt_visualize has not been enabled in this distribution");
return BT_ERROR;	
}
#endif

//\end{code}

#if (DEBUG==1) 
static void _bt_contents(t_btree *H)
{
	log_debug2("btree %d",(int) H);
	log_debug2("monitor =%d",(int) H->monitor);
	log_debug2("internal=%d",(int) H->internal);
	log_debug2("malloc  =%d",(int) H->malloc);
	log_debug2("free    =%d",(int) H->free);
	log_debug2("cmp     =%d",(int) H->cmp);
	log_debug2("in      =%d",(int) H->in);
	log_debug2("top     =%d",(int) H->top);
	log_debug2("freelist=%d",(int) H->freelist);
}
#endif



void bt_error(t_btree *H,t_btree_error e,const char *errmsg)
{
	H->errmsg=(char *) errmsg;
	H->err=e;
}

const char *bt_errmsg(t_btree *H)
{
	if (H->errmsg) { return H->errmsg; }
	else { return ""; }
}

t_btree_error bt_errno(t_btree *H)
{
	return H->err;
}


void bt_version(FILE *out){
	fprintf(out,"%s v%s%s\n",
				PROGRAM,
				VERSION,
#ifdef HAVE_THREADS
				" (Thread Support)"
#else
				""
#endif								
		   );
}	

