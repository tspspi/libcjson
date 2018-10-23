#include "../include/cjson.h"
#include <stdlib.h>

#ifdef __cplusplus
	extern "C" {
#endif


enum cjsonError cjsonNumber_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;
	struct cjsonNumber* lpNew;
	if(lpOut == NULL) { return cjsonE_InvalidParam; }

	if(lpSystem == NULL) {
		lpNew = (struct cjsonNumber*)malloc(sizeof(struct cjsonNumber));
		if(lpNew == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonNumber), (void**)(&lpNew));
		if(e != cjsonE_Ok) { return e; }
	}

	lpNew->base.type = cjsonNumber_UnsignedLong;
	lpNew->base.lpSystem = lpSystem;
	lpNew->value.ulong = 0;
	(*lpOut) = (struct cjsonValue*)lpNew;
	return cjsonE_Ok;
}
unsigned long int cjsonObject_GetAsULong(
	struct cjsonValue* lpValue
) {
	struct cjsonNumber* lpSelf = (struct cjsonNumber*)lpValue;
	if(lpValue == NULL) { return 0; }

	switch(lpValue->type) {
		case cjsonNumber_UnsignedLong:		return lpSelf->value.ulong;
		case cjsonNumber_SignedLong:		return (unsigned long)(lpSelf->value.slong);
		case cjsonNumber_Double:			return (unsigned long)(lpSelf->value.dbl);
		default:							return 0;
	}
}
signed long int cjsonObject_GetAsSLong(
	struct cjsonValue* lpValue
) {
	struct cjsonNumber* lpSelf = (struct cjsonNumber*)lpValue;
	if(lpValue == NULL) { return 0; }

	switch(lpValue->type) {
		case cjsonNumber_UnsignedLong:		return (signed long)(lpSelf->value.ulong);
		case cjsonNumber_SignedLong:		return lpSelf->value.slong;
		case cjsonNumber_Double:			return (signed long)(lpSelf->value.dbl);
		default:							return 0;
	}
}
double cjsonObject_GetAsDouble(
	struct cjsonValue* lpValue
) {
	struct cjsonNumber* lpSelf = (struct cjsonNumber*)lpValue;
	if(lpValue == NULL) { return 0; }

	switch(lpValue->type) {
		case cjsonNumber_UnsignedLong:		return (double)(lpSelf->value.ulong);
		case cjsonNumber_SignedLong:		return (double)(lpSelf->value.slong);
		case cjsonNumber_Double:			return lpSelf->value.dbl;
		default:							return 0;
	}
}
enum cjsonError cjsonNumber_SetULong(
	struct cjsonValue* lpValue,
	unsigned long int value
) {
	struct cjsonNumber* lpSelf = (struct cjsonNumber*)lpValue;
	if(lpValue == NULL) { return 0; }

	if(!cjsonIsNumeric(lpValue)) { return cjsonE_InvalidParam; }

	lpValue->type = cjsonNumber_UnsignedLong;
	lpSelf->value.ulong = value;
	return cjsonE_Ok;
}
enum cjsonError cjsonNumber_SetSLong(
	struct cjsonValue* lpValue,
	signed long int value
) {
	struct cjsonNumber* lpSelf = (struct cjsonNumber*)lpValue;
	if(lpValue == NULL) { return 0; }

	if(!cjsonIsNumeric(lpValue)) { return cjsonE_InvalidParam; }

	lpValue->type = cjsonNumber_SignedLong;
	lpSelf->value.slong = value;
	return cjsonE_Ok;
}
enum cjsonError cjsonNumber_SetDouble(
	struct cjsonValue* lpValue,
	double value
) {
	struct cjsonNumber* lpSelf = (struct cjsonNumber*)lpValue;
	if(lpValue == NULL) { return 0; }

	if(!cjsonIsNumeric(lpValue)) { return cjsonE_InvalidParam; }

	lpValue->type = cjsonNumber_Double;
	lpSelf->value.dbl = value;
	return cjsonE_Ok;
}


#ifdef __cplusplus
	} /* extern "C" { */
#endif
