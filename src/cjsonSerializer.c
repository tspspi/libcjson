#include "../include/cjson.h"
#include <stdlib.h>
#include <string.h>

/* Currently we use stdio for number output */
#include <stdio.h>

#ifdef __cplusplus
	extern "C" {
#endif

/*
	Malloc and free abstraction
*/
static inline enum cjsonError cjsonSerializer_MallocHelper(
	struct cjsonSerializer* lpSelf,
	unsigned long int dwSize,
	void** lpOut
) {
	if(lpSelf->lpSystem == NULL) {
		(*lpOut) = malloc(dwSize);
		if((*lpOut) == NULL) {
			return cjsonE_OutOfMemory;
		}
		return cjsonE_Ok;
	} else {
		return lpSelf->lpSystem->alloc(lpSelf->lpSystem, dwSize, lpOut);
	}
}
static inline void cjsonSerializer_FreeHelper(
	struct cjsonSerializer* lpSelf,
	void* lpBlock
) {
	if(lpSelf->lpSystem == NULL) {
		free(lpBlock);
	} else {
		lpSelf->lpSystem->free(lpSelf->lpSystem, lpBlock);
	}
}


static inline enum cjsonError cjsonSerializer_PushValue(
	struct cjsonSerializer* lpSerializer,
	struct cjsonValue* lpValue
) {
	struct cjsonSerializer_String* lpNewString;
	struct cjsonSerializer_Constant* lpNewConst;
	struct cjsonSerializer_Array* lpNewArray;
	struct cjsonSerializer_Object* lpNewObject;
	struct cjsonSerializer_Number* lpNewNum;

	unsigned long int dwLength;

	enum cjsonError e;

	switch(lpValue->type) {
		case cjsonObject:
			e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_Object), (void**)(&lpNewObject));
			if(e != cjsonE_Ok) { return e; }

			lpNewObject->base.type = cjsonSerializer_StackEntryType__Object;
			lpNewObject->base.lpNext = lpSerializer->lpTopOfStack;
			lpNewObject->lpObject = (struct cjsonObject*)lpValue;
			lpNewObject->dwBytesWritten = 0;
			lpNewObject->dwCurrentBucket = 0;
			lpNewObject->lpCurrentBucketEntry = 0;
			lpNewObject->state = cjsonSerializer_Object_State__Header;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewObject;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;
		case cjsonArray:
			e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_Array), (void**)(&lpNewArray));
			if(e != cjsonE_Ok) { return e; }

			lpNewArray->base.type = cjsonSerializer_StackEntryType__Array;
			lpNewArray->base.lpNext = lpSerializer->lpTopOfStack;
			lpNewArray->lpArray = (struct cjsonArray*)lpValue;
			lpNewArray->dwBytesWritten = 0;
			lpNewArray->dwCurrentIndex = 0;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewArray;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;

		case cjsonString:
			e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_String), (void**)(&lpNewString));
			if(e != cjsonE_Ok) { return e; }

			lpNewString->base.type = cjsonSerializer_StackEntryType__String;
			lpNewString->base.lpNext = lpSerializer->lpTopOfStack;
			lpNewString->lpStringObject = lpValue;
			lpNewString->state = cjsonSerializer_String_State__LeadingQuote;
			lpNewString->dwCurrentIndex = 0;
			lpNewString->dwUnicodeBytesWritten = 0;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewString;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;

		case cjsonNumber_UnsignedLong:
			/* Determine the string length */
			for(;;) {
				/* Repeat loop to cope with locale changes between both snprintf's */
				dwLength = snprintf(NULL, 0, "%lu", ((struct cjsonNumber*)lpValue)->value.ulong);
				e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_Number)+dwLength, (void**)(&lpNewNum));
				if(e != cjsonE_Ok) { return e; }
				if(snprintf(&(lpNewNum->bString[0]), dwLength, "%lu", ((struct cjsonNumber*)lpValue)->value.ulong) != dwLength) {
					cjsonSerializer_FreeHelper(lpSerializer, (void*)lpNewNum);
					continue;
				}
				lpNewNum->dwStringLen = dwLength;
				lpNewNum->dwWritten = 0;
				break;
			}
			lpNewNum->base.type = cjsonSerializer_StackEntryType__Number;
			lpNewNum->base.lpNext = lpSerializer->lpTopOfStack;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewNum;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;

		case cjsonNumber_SignedLong:
			for(;;) {
				/* Repeat loop to cope with locale changes between both snprintf's */
				dwLength = snprintf(NULL, 0, "%ld", ((struct cjsonNumber*)lpValue)->value.slong);
				e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_Number)+dwLength, (void**)(&lpNewNum));
				if(e != cjsonE_Ok) { return e; }
				if(snprintf(lpNewNum->bString, dwLength, "%ld", ((struct cjsonNumber*)lpValue)->value.ulong) != dwLength) {
					cjsonSerializer_FreeHelper(lpSerializer, (void*)lpNewNum);
					continue;
				}
				lpNewNum->dwStringLen = dwLength;
				lpNewNum->dwWritten = 0;
				break;
			}
			lpNewNum->base.type = cjsonSerializer_StackEntryType__Number;
			lpNewNum->base.lpNext = lpSerializer->lpTopOfStack;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewNum;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;

		case cjsonNumber_Double:
			for(;;) {
				/* Repeat loop to cope with locale changes between both snprintf's */
				dwLength = snprintf(NULL, 0, "%lf", ((struct cjsonNumber*)lpValue)->value.dbl);
				e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_Number)+dwLength, (void**)(&lpNewNum));
				if(e != cjsonE_Ok) { return e; }
				if(snprintf(lpNewNum->bString, dwLength, "%lf", ((struct cjsonNumber*)lpValue)->value.dbl) != dwLength) {
					cjsonSerializer_FreeHelper(lpSerializer, (void*)lpNewNum);
					continue;
				}
				lpNewNum->dwStringLen = dwLength;
				lpNewNum->dwWritten = 0;
				break;
			}
			lpNewNum->base.type = cjsonSerializer_StackEntryType__Number;
			lpNewNum->base.lpNext = lpSerializer->lpTopOfStack;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewNum;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;

		case cjsonTrue:
		case cjsonFalse:
		case cjsonNull:
			e = cjsonSerializer_MallocHelper(lpSerializer, sizeof(struct cjsonSerializer_Constant), (void**)(&lpNewConst));
			if(e != cjsonE_Ok) { return e; }

			lpNewConst->base.type = cjsonSerializer_StackEntryType__Constant;
			lpNewConst->base.lpNext = lpSerializer->lpTopOfStack;
			lpNewConst->constantType = lpValue->type;
			lpNewConst->dwBytesWritten = 0;

			lpSerializer->lpTopOfStack = (struct cjsonSerializer_StackEntry*)lpNewConst;
			lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth + 1;
			return cjsonE_Ok;

		default:
			return cjsonE_ImplementationError;
	}
}

static inline enum cjsonError cjsonSerializer_PopValue(
	struct cjsonSerializer* lpSerializer
) {
	struct cjsonSerializer_StackEntry* lpPrev;

	lpPrev = lpSerializer->lpTopOfStack;
	lpSerializer->lpTopOfStack = lpPrev->lpNext;
	lpSerializer->dwStateStackDepth = lpSerializer->dwStateStackDepth - 1;

	cjsonSerializer_FreeHelper(lpSerializer, (void*)lpPrev);
	return cjsonE_Ok;
}

static char* strTrue = "true";
static char* strFalse = "false";
static char* strNull = "null";

static inline enum cjsonError cjsonSerializer_Continue_Constant(
	struct cjsonSerializer* lpSerializer
) {
	enum cjsonError e;
	char* lpConst;
	unsigned long int dwConstLen;
	unsigned long int dwBytesWritten;

	struct cjsonSerializer_Constant* lpCur = (struct cjsonSerializer_Constant*)(lpSerializer->lpTopOfStack);

	switch(lpCur->constantType) {
		case cjsonTrue:		lpConst = strTrue; 	dwConstLen = strlen(strTrue); 	break;
		case cjsonFalse:	lpConst = strFalse; dwConstLen = strlen(strFalse); 	break;
		case cjsonNull:		lpConst = strNull; 	dwConstLen = strlen(strNull);	break;

		default:			return cjsonE_ImplementationError;
	}

	while(lpCur->dwBytesWritten < dwConstLen) {
		dwBytesWritten = 0;
		e = lpSerializer->callbackWriteBytes(&(lpConst[lpCur->dwBytesWritten]), (dwConstLen - lpCur->dwBytesWritten), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		lpCur->dwBytesWritten = lpCur->dwBytesWritten + dwBytesWritten;
		if(e != cjsonE_Ok) { return e; }
	}

	return cjsonSerializer_PopValue(lpSerializer);
}
static inline enum cjsonError cjsonSerializer_Continue_Number(
	struct cjsonSerializer* lpSerializer
) {
	unsigned long int dwBytesWritten;
	enum cjsonError e;

	struct cjsonSerializer_Number* lpCur = (struct cjsonSerializer_Number*)(lpSerializer->lpTopOfStack);
	for(;;) {
		dwBytesWritten = 0;
		e = lpSerializer->callbackWriteBytes(&(lpCur->bString[lpCur->dwWritten]), (lpCur->dwStringLen - lpCur->dwWritten), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		lpCur->dwWritten = lpCur->dwWritten + dwBytesWritten;
		if(e != cjsonE_Ok) { return e; }

		if(lpCur->dwWritten == lpCur->dwStringLen) {
			break;
		}
	}

	return cjsonSerializer_PopValue(lpSerializer);
}
static inline enum cjsonError cjsonSerializer_Continue_Array(
	struct cjsonSerializer* lpSerializer
) {
	enum cjsonError e;

	struct cjsonSerializer_Array* lpCur = (struct cjsonSerializer_Array*)(lpSerializer->lpTopOfStack);
	unsigned long int dwBytesWritten;
	unsigned long int dwHeaderTrailerIndentions;
	unsigned long int dwValueIndentions;
	struct cjsonValue* lpValue;
	char bNext;

	dwHeaderTrailerIndentions = ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0) ? lpSerializer->dwStateStackDepth : 0;
	dwValueIndentions = ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0) ? dwHeaderTrailerIndentions + 1 : 0;

	/* Have we already emitted the header? */
	if(lpCur->dwBytesWritten < 1) {
		lpCur->dwWrittenIndent = 0;
		bNext = '[';
		dwBytesWritten = 0;
		e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
		if(e != cjsonE_Ok) { return e; }
	}
	if((lpCur->dwBytesWritten < 2) && ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0)) {
		bNext = '\n';
		dwBytesWritten = 0;
		e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
		if(e != cjsonE_Ok) { return e; }
	}

	/* Now iterate over all our remaining elements */
	if(lpCur->dwCurrentIndex < lpCur->lpArray->dwElementCount) {
		if((lpCur->dwCurrentIndex > 0) && (lpCur->dwWrittenPast == 0)) {
			bNext = ',';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwWrittenPast = lpCur->dwWrittenPast + 1; }
			if(e != cjsonE_Ok) { return e; }
		}
		if((lpCur->dwCurrentIndex > 0) && (lpCur->dwWrittenPast == 1) && ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0)) {
			bNext = '\n';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwWrittenPast = lpCur->dwWrittenPast + 1; }
			if(e != cjsonE_Ok) { return e; }
		}

		if(lpCur->dwWrittenIndent < dwValueIndentions) {
			bNext = '\t';
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwWrittenIndent = lpCur->dwWrittenIndent + 1; }
			if(e != cjsonE_Ok) { return e; }
		}

		/* Get element ... */
		if((e = cjsonArray_Get((struct cjsonValue*)(lpCur->lpArray), lpCur->dwCurrentIndex, &lpValue)) != cjsonE_Ok) { return e; }

		/* Push onto stack and process ... */
		e = cjsonSerializer_PushValue(lpSerializer, lpValue);
		if(e != cjsonE_Ok) { return e; }
		lpCur->dwWrittenPast = 0;
		lpCur->dwWrittenIndent = 0;

		/*
			To process we increment our index (for later on - we will
			get restarted for every element!) and return to the continuation
			function after our child has finished
		*/
		lpCur->dwCurrentIndex = lpCur->dwCurrentIndex + 1;
		return cjsonE_Ok;
	}

	/*
		All entries have been written ... add:
			linebreak (pretty print only)
			indention (pretty print only)
			closing brace
			linebreak (pretty print only)
	*/
	if((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0)  {
		if(lpCur->dwBytesWritten < 3) {
			bNext = '\n';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
			if(e != cjsonE_Ok) { return e; }
		}
		while(lpCur->dwBytesWritten < (dwHeaderTrailerIndentions+2)) {
			bNext = '\t';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
			if(e != cjsonE_Ok) { return e; }
		}

		bNext = ']';
		dwBytesWritten = 0;
		e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
		if(e != cjsonE_Ok) { return e; }

		return cjsonSerializer_PopValue(lpSerializer);
	} else {
		bNext = ']';
		dwBytesWritten = 0;
		e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
		if(e != cjsonE_Ok) { return e; }
		return cjsonSerializer_PopValue(lpSerializer);
	}
}
static inline enum cjsonError cjsonSerializer_Continue_Object(
	struct cjsonSerializer* lpSerializer
) {
	enum cjsonError e;

	struct cjsonSerializer_Object* lpCur = (struct cjsonSerializer_Object*)(lpSerializer->lpTopOfStack);

	unsigned long int dwBytesWritten;
	unsigned long int dwIndentDepth;
	char bNext;

	dwIndentDepth = ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0) ? lpSerializer->dwStateStackDepth : 0;
	/*
		The header includes indention an an brace
	*/
	if(lpCur->state == cjsonSerializer_Object_State__Header) {
		if(lpCur->dwBytesWritten < 1) {
			bNext = '{';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
			if(e != cjsonE_Ok) { return e; }
		}
		if((lpCur->dwBytesWritten < 2) && ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0)) {
			bNext = '\n';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
			if(e != cjsonE_Ok) { return e; }
		}

		/*
			Select the first object entry in the first bucket as
			the "next" one that should be written and switch to key state
		*/
		lpCur->lpCurrentBucketEntry = NULL;
		for(lpCur->dwCurrentBucket = 0; lpCur->dwCurrentBucket < lpCur->lpObject->dwBucketCount; lpCur->dwCurrentBucket = lpCur->dwCurrentBucket + 1) {
			if(lpCur->lpObject->buckets[lpCur->dwCurrentBucket] != NULL) {
				lpCur->lpCurrentBucketEntry = lpCur->lpObject->buckets[lpCur->dwCurrentBucket];
				break;
			}
		}

		if(lpCur->lpCurrentBucketEntry != NULL) {
			lpCur->state = cjsonSerializer_Object_State__Key;
			lpCur->dwBytesWritten = 0;
			lpCur->lpTempString = NULL;
		} else {
			lpCur->state = cjsonSerializer_Object_State__Trailer;
			lpCur->dwBytesWritten = 0;
		}
	}

	/*
		If we are in key state we have to write all key bytes (including indention quotes and trailing colon)
	*/

	for(;;) {
		if(lpCur->state == cjsonSerializer_Object_State__Key) {
			/* Check if we have to write indent ... */
			while(lpCur->dwBytesWritten < dwIndentDepth) {
				bNext = '\t';
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
				if(e != cjsonE_Ok) { return e; }
			}

			if(lpCur->dwBytesWritten < dwIndentDepth+1) {
				if(lpCur->lpTempString == NULL) {
					e = cjsonString_Create(&lpCur->lpTempString, lpCur->lpCurrentBucketEntry->bKey, lpCur->lpCurrentBucketEntry->dwKeyLength, lpSerializer->lpSystem);
					if(e != cjsonE_Ok) { return e; }
				}
				e = cjsonSerializer_PushValue(lpSerializer, lpCur->lpTempString);
				if(e != cjsonE_Ok) { return e; }
				lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1;
				return cjsonE_Ok; /* Continuation will output this string */
			}

			if(lpCur->dwBytesWritten < dwIndentDepth +2 ) {
				cjsonReleaseValue(lpCur->lpTempString);
				lpCur->lpTempString = NULL;
				lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1;
			}

			if(lpCur->dwBytesWritten < dwIndentDepth + 3) {
				bNext = ':';
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
				if(e != cjsonE_Ok) { return e; }
			}

			/* When we reached here switch to value state ... */
			e = cjsonSerializer_PushValue(lpSerializer, lpCur->lpCurrentBucketEntry->lpValue);
			if(e != cjsonE_Ok) { return e; }
			lpCur->state = cjsonSerializer_Object_State__Value;
			lpCur->dwBytesWritten = 0;
			return cjsonE_Ok; /* Exit to continuation function */
		}

		/*
			In case we write the value we do that recursively. IF we can then select
			another bucket entry as the next one we have to write an comma as separator
		*/
		if(lpCur->state == cjsonSerializer_Object_State__Value) {
			/*
				The value has been written by it's own continuation function
				and we've returned into our state. Write comma if a new value can be
				selected
			*/
			if(lpCur->dwBytesWritten == 0) {
				if(lpCur->lpCurrentBucketEntry->bucketList.lpNext != NULL) {
					lpCur->lpCurrentBucketEntry = lpCur->lpCurrentBucketEntry->bucketList.lpNext;
				} else {
					/* We have to switch to the next non-emtpy bucket - if any */
					lpCur->dwCurrentBucket = lpCur->dwCurrentBucket + 1;
					lpCur->lpCurrentBucketEntry = NULL;
					while(lpCur->dwCurrentBucket < lpCur->lpObject->dwBucketCount) {
						if(lpCur->lpObject->buckets[lpCur->dwCurrentBucket] != NULL) {
							lpCur->lpCurrentBucketEntry = lpCur->lpObject->buckets[lpCur->dwCurrentBucket];
							break;
						}
						lpCur->dwCurrentBucket = lpCur->dwCurrentBucket + 1;
					}
				}

				lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1;
			}

			if(lpCur->lpCurrentBucketEntry != NULL) {
				if(lpCur->dwBytesWritten < 2) {
					/* Comma and optional linebreak */
					bNext = ',';
					dwBytesWritten = 0;
					e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
					if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
					if(e != cjsonE_Ok) { return e; }
				}

				if((lpCur->dwBytesWritten < 3) && ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0)) {
					bNext = '\n';
					dwBytesWritten = 0;
					e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
					if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
					if(e != cjsonE_Ok) { return e; }
				}

				lpCur->state = cjsonSerializer_Object_State__Key;
				lpCur->dwBytesWritten = 0;
				return cjsonE_Ok;
			}

			/* We are finished ... */
			if((lpCur->dwBytesWritten < 2) && ((lpSerializer->dwFlags & CJSON_SERIALIZER__FLAG__PRETTYPRINT) != 0)) {
				bNext = '\n';
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
				if(e != cjsonE_Ok) { return e; }
			}
			lpCur->state = cjsonSerializer_Object_State__Trailer;
			lpCur->dwBytesWritten = 0;
			break;
		}
	}

	/* After we are done we write the trailer */

	if(lpCur->state == cjsonSerializer_Object_State__Trailer) {
		while(lpCur->dwBytesWritten+1 < dwIndentDepth) {
			bNext = '\t';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
			if(e != cjsonE_Ok) { return e; }
		}
		if(lpCur->dwBytesWritten < dwIndentDepth+1) {
			bNext = '}';
			dwBytesWritten = 0;
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			if(dwBytesWritten > 0) { lpCur->dwBytesWritten = lpCur->dwBytesWritten + 1; }
			if(e != cjsonE_Ok) { return e; }
		}

	}

	/* When we are done we pop ourself off the stack ... */
	return cjsonSerializer_PopValue(lpSerializer);
}
static inline enum cjsonError cjsonSerializer_Continue_String(
	struct cjsonSerializer* lpSerializer
) {
	enum cjsonError e;

	struct cjsonSerializer_String* lpCur = (struct cjsonSerializer_String*)(lpSerializer->lpTopOfStack);
	struct cjsonString* lpString;

	unsigned long int dwBytesWritten;
	char bNext;
	uint8_t b1, b2, b3;
	uint16_t uChar;

	/* Check if the next character has to be escaped or is a multi byte character */
	lpString = (struct cjsonString*)lpCur->lpStringObject;

	if(lpCur->state == cjsonSerializer_String_State__LeadingQuote) {
		bNext = '"';
		e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		if(dwBytesWritten > 0) {
			lpCur->state = cjsonSerializer_String_State__Normal;
		}
		if(e != cjsonE_Ok) { return e; }
	}

	while((lpCur->dwCurrentIndex < lpString->dwStrlen) || (lpCur->state == cjsonSerializer_String_State__Unicode)) {
		if(lpCur->state == cjsonSerializer_String_State__Normal) {
			/* Fetch next byte and check if it is special */
			bNext = lpString->bData[lpCur->dwCurrentIndex];
			if((bNext & 0x80) != 0) {
				/* Next character requires unicode encoding. First prepare bytes, then cache */
				if((bNext & 0xF8) == 0xF0) {
					/* This is not representable via \uXXXX notation */
					return cjsonE_EncodingError;
				} else if((bNext & 0xF0) == 0xE0) {
					/* Three bytes are used to encode the value */
					if(lpString->dwStrlen - lpCur->dwCurrentIndex < 3) { return cjsonE_InvalidParam; }

					b1 = lpString->bData[lpCur->dwCurrentIndex];
					b2 = lpString->bData[lpCur->dwCurrentIndex+1];
					b3 = lpString->bData[lpCur->dwCurrentIndex+2];

					if(((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)) { return cjsonE_InvalidParam; }

					uChar = ((((uint16_t)b1) & 0x0F) << 12) | ((((uint16_t)b2) & 0x3F) << 6) | (((uint16_t)b3) & 0x3F);

					lpCur->dwCurrentIndex = lpCur->dwCurrentIndex + 3;
				} else if((bNext & 0xE0) == 0xC0) {
					/* Two bytes are used to encode the value */
					if(lpString->dwStrlen - lpCur->dwCurrentIndex < 2) { return cjsonE_InvalidParam; }

					b1 = lpString->bData[lpCur->dwCurrentIndex];
					b2 = lpString->bData[lpCur->dwCurrentIndex+1];
					if((b2 & 0xC0) != 0x80) { return cjsonE_InvalidParam; }

					uChar = ((((uint16_t)b1) & 0x1F) << 6) | (((uint16_t)b2) & 0x3F);

					lpCur->dwCurrentIndex = lpCur->dwCurrentIndex + 2;
				} else {
					return cjsonE_EncodingError;
				}

				lpCur->bUnicodeBytes[0] = (uint8_t)((uChar & 0xF000) >> 12);
				lpCur->bUnicodeBytes[1] = (uint8_t)((uChar & 0x0F00) >> 8);
				lpCur->bUnicodeBytes[2] = (uint8_t)((uChar & 0x00F0) >> 4);
				lpCur->bUnicodeBytes[3] = (uint8_t)(uChar & 0x000F);

				lpCur->bUnicodeBytes[0] = lpCur->bUnicodeBytes[0] + ((lpCur->bUnicodeBytes[0] > 9) ? 'A'-10 : '0');
				lpCur->bUnicodeBytes[1] = lpCur->bUnicodeBytes[1] + ((lpCur->bUnicodeBytes[1] > 9) ? 'A'-10 : '0');
				lpCur->bUnicodeBytes[2] = lpCur->bUnicodeBytes[2] + ((lpCur->bUnicodeBytes[2] > 9) ? 'A'-10 : '0');
				lpCur->bUnicodeBytes[3] = lpCur->bUnicodeBytes[3] + ((lpCur->bUnicodeBytes[3] > 9) ? 'A'-10 : '0');

				lpCur->dwUnicodeBytesWritten = 0;

				lpCur->state = cjsonSerializer_String_State__Unicode;

				return cjsonE_Ok;
			} else if((bNext == '"') || (bNext == '\\') || (bNext == '/') || (bNext == '\b') || (bNext == '\f') || (bNext == '\n') || (bNext == '\r') || (bNext == '\t')) {
				/* Requires escaping ... we write an escape character first */
				bNext = '\\';
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->state = cjsonSerializer_String_State__Escaped; }
				if(e != cjsonE_Ok) { return e; }
				continue;
			} else {
				/* Direct write */
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				lpCur->dwCurrentIndex = lpCur->dwCurrentIndex + dwBytesWritten;
				if(e != cjsonE_Ok) { return e; }
				continue;
			}
		} else if(lpCur->state == cjsonSerializer_String_State__Escaped) {
			bNext = lpString->bData[lpCur->dwCurrentIndex];
			switch(bNext) {
				case '"':		break;
				case '\\':		break;				case '/':		break;
				case '\b':		bNext = 'b'; break;
				case '\f':		bNext = 'f'; break;
				case '\n':		bNext = 'n'; break;
				case '\r':		bNext = 'r'; break;
				case '\t':		bNext = 't'; break;
				default:		return cjsonE_ImplementationError;
			}
			e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
			lpCur->dwCurrentIndex = lpCur->dwCurrentIndex + dwBytesWritten;
			if(dwBytesWritten > 0) { lpCur->state = cjsonSerializer_String_State__Normal; }
			if(e != cjsonE_Ok) { return e; }
			continue;
		} else if(lpCur->state == cjsonSerializer_String_State__Unicode) {
			if(lpCur->dwUnicodeBytesWritten == 0) {
				bNext = '\\';
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->dwUnicodeBytesWritten = lpCur->dwUnicodeBytesWritten + 1; }
				if(e != cjsonE_Ok) { return e; }
			}
			if(lpCur->dwUnicodeBytesWritten == 1) {
				bNext = 'u';
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->dwUnicodeBytesWritten = lpCur->dwUnicodeBytesWritten + 1; }
				if(e != cjsonE_Ok) { return e; }
			}
			while(lpCur->dwUnicodeBytesWritten < 2+4) {
				bNext = lpCur->bUnicodeBytes[lpCur->dwUnicodeBytesWritten-2];
				dwBytesWritten = 0;
				e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
				if(dwBytesWritten > 0) { lpCur->dwUnicodeBytesWritten = lpCur->dwUnicodeBytesWritten + 1; }
				if(e != cjsonE_Ok) { return e; }
			}
			lpCur->dwUnicodeBytesWritten = 0;
			lpCur->state = cjsonSerializer_String_State__Normal;

			return cjsonE_Ok;
		} else {
			return cjsonE_ImplementationError;
		}
	}

	if(lpCur->dwCurrentIndex == lpString->dwStrlen) {
		bNext = '"';
		e = lpSerializer->callbackWriteBytes(&bNext, sizeof(bNext), &dwBytesWritten, lpSerializer->callbackWriteBytesParam);
		lpCur->dwCurrentIndex = lpCur->dwCurrentIndex + dwBytesWritten;
		if(e != cjsonE_Ok) { return e; }
	}

	return cjsonSerializer_PopValue(lpSerializer);
}








enum cjsonError cjsonSerializer_Create(
	struct cjsonSerializer** lpOut,
	cjsonSerializer_Callback_WriteBytes callback,
	void* callbackFreeParam,
	uint32_t dwFlags,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;
	struct cjsonSerializer* lpNew;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if(callback == NULL) { return cjsonE_InvalidParam; }

	if(lpSystem == NULL) {
		lpNew = (struct cjsonSerializer*)malloc(sizeof(struct cjsonSerializer));
		if(lpNew == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonSerializer), (void**)(&lpNew));
		if(e != cjsonE_Ok) { return e; }
	}

	lpNew->lpTopOfStack 			= NULL;
	lpNew->lpSystem 				= lpSystem;
	lpNew->callbackWriteBytes 		= callback;
	lpNew->callbackWriteBytesParam 	= callbackFreeParam;
	lpNew->dwStateStackDepth		= 0;
	lpNew->dwFlags					= dwFlags;

	(*lpOut) = lpNew;
	return cjsonE_Ok;
}
enum cjsonError cjsonSerializer_Serialize(
	struct cjsonSerializer* lpSerializer,
	struct cjsonValue* lpValue
) {
	enum cjsonError e;

	if(lpSerializer == NULL) { return cjsonE_InvalidParam; }

	/*
		We can only start the next serialization after we've
		finished the last one.
	*/
	if(lpSerializer->lpTopOfStack != NULL) { return cjsonE_InvalidState; }

	e = cjsonSerializer_PushValue(lpSerializer, lpValue);
	if(e != cjsonE_Ok) { return e; }
	return cjsonSerializer_Continue(lpSerializer);
}

enum cjsonError cjsonSerializer_Continue(
	struct cjsonSerializer* lpSerializer
) {
	enum cjsonError e;
	if(lpSerializer == NULL) { return cjsonE_InvalidParam; }
	if(lpSerializer->lpTopOfStack == NULL) { return cjsonE_Ok; }

	while(lpSerializer->lpTopOfStack != NULL) {
		switch(lpSerializer->lpTopOfStack->type) {
			case cjsonSerializer_StackEntryType__Array:				e = cjsonSerializer_Continue_Array(lpSerializer);		break;
			case cjsonSerializer_StackEntryType__Constant:			e = cjsonSerializer_Continue_Constant(lpSerializer); 	break;
			case cjsonSerializer_StackEntryType__Number:			e = cjsonSerializer_Continue_Number(lpSerializer); 		break;
			case cjsonSerializer_StackEntryType__Object:			e = cjsonSerializer_Continue_Object(lpSerializer);		break;
			case cjsonSerializer_StackEntryType__String:			e = cjsonSerializer_Continue_String(lpSerializer);		break;
			default:
				return cjsonE_ImplementationError;
		}
		if(e != cjsonE_Ok) { return e; }
	}

	return cjsonE_Ok;
}
enum cjsonError cjsonSerializer_Release(
	struct cjsonSerializer* lpSerializer
) {
	struct cjsonSerializer_StackEntry* lpCur;
	struct cjsonSerializer_StackEntry* lpNext;

	if(lpSerializer == NULL) { return cjsonE_InvalidParam; }

	lpCur = lpSerializer->lpTopOfStack;
	while(lpCur != NULL) {
		lpNext = lpCur->lpNext;

		switch(lpCur->type) {
			case cjsonSerializer_StackEntryType__Array:			cjsonSerializer_FreeHelper(lpSerializer, (void*)lpCur); break;
			case cjsonSerializer_StackEntryType__Constant:		cjsonSerializer_FreeHelper(lpSerializer, (void*)lpCur); break;
			case cjsonSerializer_StackEntryType__Number:		return cjsonE_ImplementationError;
			case cjsonSerializer_StackEntryType__Object:
				if(((struct cjsonSerializer_Object*)lpCur)->lpTempString != NULL) {
					cjsonReleaseValue(((struct cjsonSerializer_Object*)lpCur)->lpTempString);
					((struct cjsonSerializer_Object*)lpCur)->lpTempString = NULL;
				}
				cjsonSerializer_FreeHelper(lpSerializer, (void*)lpCur);
				break;
			case cjsonSerializer_StackEntryType__String:		cjsonSerializer_FreeHelper(lpSerializer, (void*)lpCur); break;
			default:
				return cjsonE_ImplementationError;
		}

		lpCur = lpNext;
	}

	cjsonSerializer_FreeHelper(lpSerializer, (void*)lpSerializer);
	return cjsonE_Ok;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
