/*
   This file is part of mp3cuefuse.
   Copyright 2013, Hans Oesterholt <debian@oesterholt.net>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with elementals.  If not, see <http://www.gnu.org/licenses/>.

   ********************************************************************
*/

#include "segmenter.h"
#include "log.h"
#include <stdio.h>

FILE * log_handle() {
	return stderr;
}

int main() {
	segmenter_t * S=segmenter_new();
	segmenter_prepare(S,"/tmp/10cc.mp3",9,"9","a9","dreadlock","10cc","-","Pop",1970,"",60*1000,120*1000);
	segmenter_create(S);
	FILE *fout=fopen("/tmp/10cca.mp3","wb");
	log_debug2("opening %d",segmenter_open(S));
	FILE *fin=segmenter_stream(S);
	log_debug3("fin=%p, fout=%p",fin,fout);
    int s;
    char buf[1024];
    while ((s=fread(buf,1,1024,fin))>0) {
       fwrite(buf,s,1,fout);
    }
    segmenter_close(S);
	fclose(fout);
	segmenter_destroy(S);
	return 0;
}
