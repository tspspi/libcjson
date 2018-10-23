#include "../include/cjson.h"
#include <stdlib.h>
#include <string.h>	/* used for memcpy */

#ifdef __cplusplus
	extern "C" {
#endif


enum cjsonError cjsonString_Create(
	struct cjsonValue** lpStringOut,
	const char* lpData,
	unsigned long int dwDataLength,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;
	struct cjsonString* lpNew;

	if(lpStringOut == NULL) { return cjsonE_InvalidParam; }
	(*lpStringOut) = NULL;

	if(lpSystem == NULL) {
		lpNew = (struct cjsonString*)malloc(sizeof(struct cjsonString)+dwDataLength);
		if(lpNew == NULL) {
			return cjsonE_OutOfMemory;
		}
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonString)+dwDataLength, (void**)(&lpNew));
		if(e != cjsonE_Ok) { return e; }
	}

	lpNew->base.type = cjsonString;
	lpNew->base.lpSystem = lpSystem;
	lpNew->dwStrlen = dwDataLength;
	memcpy(lpNew->bData, lpData, dwDataLength);

	(*lpStringOut) = (struct cjsonValue*)lpNew;
	return cjsonE_Ok;
}
char* cjsonString_Get(
	struct cjsonValue* lpValue
) {
	if(lpValue == NULL) { return NULL; }
	if(lpValue->type != cjsonString) { return NULL; }

	return ((struct cjsonString*)lpValue)->bData;
}
unsigned long int cjsonString_Strlen(
	struct cjsonValue* lpValue
) {
	if(lpValue == NULL) { return 0; }
	if(lpValue->type != cjsonString) { return 0; }

	return ((struct cjsonString*)lpValue)->dwStrlen;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
