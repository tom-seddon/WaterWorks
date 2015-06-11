#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include "debug.h"

/* Win32 string resource functions are erally naff, this is a bit better in my view. Then
   I realised string table ids are contiguous, starting from 0. oh dear. */

#define START_BUF_SIZE (1024)

typedef struct {
	UINT id;
	char *string;
}str_t;

static str_t *strings=0;
static unsigned num_strings=0;
static int atexit_done=0;

static int _cdecl str_t_cmp(const void *e1,const void *e2) {
	return ((str_t *)e1)->id-((str_t *)e2)->id;
}

static int _cdecl str_t_cmp2(const void *e1,const void *e2) {
	return *((unsigned *)e1)-((str_t *)e2)->id;
}

static void _cdecl free_strings(void) {
	unsigned i;

	dprintf("free_strings called.\n");
	for(i=0;i<num_strings;i++) {
		free(strings[i].string);
	}
	free(strings);
	num_strings=0;
	strings=0;
}

/*
	char *get_string(unsigned id)

	Retrieves a string from the application's string table, and returns a pointer to it.

	Input
		id					the ID of the string to retrieve

	Return
		char *				pointer to text of requested string, or 0 if:

							*	the specified id is invalid
							*	there was insufficient memory to allocate a buffer to store the string

	Notes
	
	*	The return is char * for convenience, but should be considered const char * pointing to a static
		data area.
	*	if char *p,*q && p=get_string(X) && q=get_string(Y) && X==Y then p==q.
*/
char *get_string(unsigned id) {
	str_t *p;
	char *buf;
	int i;
	str_t *tmp;
	static char tmpbuf[START_BUF_SIZE];

	p=bsearch(&id,strings,num_strings,sizeof(str_t),str_t_cmp2);
	if(p) {
		return p->string;
	}
	tmpbuf[0]=0;
	i=LoadString(GetModuleHandle(0),id,tmpbuf,sizeof(tmpbuf)-1);
	if(!i) {
		return 0;
	}
	tmp=realloc(strings,(num_strings+1)*sizeof(str_t));
	if(!tmp) {
		return 0;
	}
	if(!atexit_done) {
		atexit(free_strings);
		atexit_done=1;
	}
	strings=tmp;
	buf=_strdup(tmpbuf);
	if(!buf) {
		return 0;
	}
	p=&strings[num_strings++];
	p->id=id;
	p->string=buf;
	qsort(strings,num_strings,sizeof(str_t),str_t_cmp);
	return buf;
}