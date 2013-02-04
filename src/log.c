/*
 * log.c
 *
 *  Created on: 3 okt. 2011
 *      Author: hans
 */

#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *f=NULL;

// TODO: Deze code moet robuuster!
FILE *log_handle(void) {
	if (f==NULL) {
		char fn[8192];
		sprintf(fn,"%s/.syncfuse/syncfuse.log",getenv("HOME"));
		f=fopen(fn,"wt");
	}
	return f;
}

void reset_log_handle_to_space(const char *space) {
	if (f!=NULL) {
		fclose(f);
	}
	char fn[8192];
	sprintf(fn,"%s/.syncfuse/%s.log",getenv("HOME"),space);
	f=fopen(fn,"wt");
}
