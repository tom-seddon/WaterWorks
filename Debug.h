#ifndef TOM_DEBUG_H
#define TOM_DEBUG_H

#if defined(_DEBUG) || defined(ALLOW_DPRINTF)
void dprintf(char *fmt,...);
#else
/*static _inline void dprintf(char *fmt,...) {
	return;
}*/
/* This generates a warning for each dprintf invocation. But, it seems VC can't
	tell that the above dprintf does nothing. So...
	(this saves ~2K on otherwise unused strings)
*/
#define dprintf __noop
#endif

#include <ddraw.h>
#if defined(_DEBUG) || defined(DESCRIBE_DX_ERRORS)
char *describe_dx_error(HRESULT);
#else
/* Same as above goes for this. */
#define describe_dx_error(X) ""
#endif

#endif
