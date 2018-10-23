#include "../include/cjson.h"
#include <stdlib.h>

#ifdef __cplusplus
	extern "C" {
#endif

enum cjsonError cjsonTrue_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if(lpSystem == NULL) {
		(*lpOut) = (struct cjsonValue*)malloc(sizeof(struct cjsonValue));
		if((*lpOut) == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonValue), (void**)lpOut);
		if(e != cjsonE_Ok) { return e; }
	}

	(*lpOut)->type 		= cjsonTrue;
	(*lpOut)->lpSystem 	= lpSystem;

	return cjsonE_Ok;
}
enum cjsonError cjsonFalse_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if(lpSystem == NULL) {
		(*lpOut) = (struct cjsonValue*)malloc(sizeof(struct cjsonValue));
		if((*lpOut) == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonValue), (void**)lpOut);
		if(e != cjsonE_Ok) { return e; }
	}

	(*lpOut)->type 		= cjsonFalse;
	(*lpOut)->lpSystem 	= lpSystem;

	return cjsonE_Ok;
}
enum cjsonError cjsonNull_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if(lpSystem == NULL) {
		(*lpOut) = (struct cjsonValue*)malloc(sizeof(struct cjsonValue));
		if((*lpOut) == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonValue), (void**)lpOut);
		if(e != cjsonE_Ok) { return e; }
	}

	(*lpOut)->type 		= cjsonNull;
	(*lpOut)->lpSystem 	= lpSystem;

	return cjsonE_Ok;
}
enum cjsonError cjsonBoolean_Set(
	struct cjsonValue* lpValue,
	int value
) {
	if(lpValue == NULL) { return cjsonE_InvalidParam; }

	if((lpValue->type != cjsonTrue) && (lpValue->type != cjsonFalse)) { return cjsonE_InvalidParam; }

	lpValue->type = (value == 0) ? cjsonFalse : cjsonTrue;
	return cjsonE_Ok;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
