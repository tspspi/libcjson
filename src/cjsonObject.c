#include "../include/cjson.h"
#include <stdlib.h>
#include <string.h>

#ifndef CJSON_BLOCKSIZE_OBJECT
	#define CJSON_BLOCKSIZE_OBJECT 64
#endif

#ifdef __cplusplus
	extern "C" {
#endif

const unsigned long int cjsonObject_BlockSize = CJSON_BLOCKSIZE_OBJECT;

enum cjsonError cjsonObject_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;
	struct cjsonObject* lpNew;
	unsigned long int dwIndexBitCount;
	unsigned long int dwIndexTest;
	unsigned long int i;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if(lpSystem == NULL) {
		lpNew = (struct cjsonObject*)malloc(sizeof(struct cjsonObject)+sizeof(struct cjsonObject_BucketEntry)*CJSON_BLOCKSIZE_OBJECT);
		if(lpNew == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonObject)+sizeof(struct cjsonObject_BucketEntry)*CJSON_BLOCKSIZE_OBJECT, (void**)(&lpNew));
		if(e != cjsonE_Ok) { return e; }
	}

	lpNew->base.type 		= cjsonObject;
	lpNew->base.lpSystem 	= lpSystem;
	lpNew->dwBucketCount 	= CJSON_BLOCKSIZE_OBJECT;
	lpNew->dwElementCount	= 0;

	dwIndexTest = 1;
	dwIndexBitCount = 0;
	while(dwIndexTest < CJSON_BLOCKSIZE_OBJECT) {
		dwIndexTest = dwIndexTest << 1;
		dwIndexBitCount = dwIndexBitCount + 1;
	}

	lpNew->dwBucketShiftBytes = (dwIndexBitCount / 8) + ((dwIndexBitCount % 8 == 0) ? 0 : 1);

	for(i = 0; i < CJSON_BLOCKSIZE_OBJECT; i=i+1) { lpNew->buckets[i] = NULL; }

	(*lpOut) = (struct cjsonValue*)lpNew;
	return cjsonE_Ok;
}

static unsigned long int getHashIndex(
	const struct cjsonObject* lpObj,
	const char* lpKey,
	unsigned long int dwKeyLen
) {
	unsigned long int dwKeyIndex;
	unsigned long int dwShiftReg = 0;
	unsigned long int dwResult = 0;
	unsigned long int cnt = 0;

	for(dwKeyIndex = 0; dwKeyIndex < dwKeyLen; dwKeyIndex = dwKeyIndex + 1) {
		dwShiftReg = (dwShiftReg << 8) | ((unsigned long int)(lpKey[dwKeyIndex]));
		if((cnt = cnt + 1) >= lpObj->dwBucketShiftBytes) {
			dwResult = dwResult ^ dwShiftReg;
			dwShiftReg = 0;
			cnt = 0;
		}
	}
	if(cnt > 0) {
		dwResult = dwResult ^ dwShiftReg;
	}

	return dwResult % lpObj->dwBucketCount;
}

enum cjsonError cjsonObject_Set(
	struct cjsonValue* lpObject,
	const char* lpKey,
	unsigned long int dwKeyLength,
	struct cjsonValue* lpValue
) {
	enum cjsonError e;
	unsigned long int idx;
	struct cjsonObject_BucketEntry* lpNewEnt;
	struct cjsonObject_BucketEntry* lpCur;
	struct cjsonObject_BucketEntry* lpCurPrev;

	struct cjsonObject* lpObj = (struct cjsonObject*)lpObject;

	if(lpObject == NULL) { return cjsonE_InvalidParam; }
	if(lpObject->type != cjsonObject) { return cjsonE_InvalidParam; }

	/* Create a new bucket member */
	if(lpValue != NULL) {
		if(lpObj->base.lpSystem == NULL) {
			lpNewEnt = (struct cjsonObject_BucketEntry*)malloc(sizeof(struct cjsonObject_BucketEntry)+dwKeyLength);
			if(lpNewEnt == NULL) { return cjsonE_OutOfMemory; }
		} else {
			e = lpObj->base.lpSystem->alloc(lpObj->base.lpSystem, sizeof(struct cjsonObject_BucketEntry)+dwKeyLength, (void**)(&lpNewEnt));
			if(e != cjsonE_Ok) { return e; }
		}
		lpNewEnt->bucketList.lpNext = NULL;
		lpNewEnt->bucketList.lpPrev = NULL;
		lpNewEnt->lpValue = lpValue;
		lpNewEnt->dwKeyLength = dwKeyLength;
		memcpy(lpNewEnt->bKey, lpKey, dwKeyLength);
	} else {
		lpNewEnt = NULL;
	}

	idx = getHashIndex(lpObj, lpKey, dwKeyLength);

	if(lpObj->buckets[idx] == NULL) {
		if(lpNewEnt == NULL) { return cjsonE_Ok; }

		/* Simply insert a new entry */
		lpObj->buckets[idx] = lpNewEnt;
		lpObj->dwElementCount = lpObj->dwElementCount + 1;
		return cjsonE_Ok;
	} else {
		/* Append that entry to the end of the chain OR replace an existing one */
		lpCurPrev = NULL;
		lpCur = lpObj->buckets[idx];
		while(lpCur != NULL) {
			if(lpCur->dwKeyLength == lpNewEnt->dwKeyLength) {
				if(memcmp(lpCur->bKey, lpNewEnt->bKey, lpCur->dwKeyLength) == 0) {
					/* Matching key already exists ... overwrite or release */
					if(lpNewEnt == NULL) {
						/* Release the entry without replacing it ... */
						cjsonReleaseValue(lpCur->lpValue);

						/* Unlink descriptor from bucket list */
						if(lpCur->bucketList.lpPrev == NULL) {
							lpObj->buckets[idx] = lpCur->bucketList.lpNext;
						} else {
							lpCur->bucketList.lpPrev->bucketList.lpNext = lpCur->bucketList.lpNext;
						}
						if(lpCur->bucketList.lpNext != NULL) {
							lpCur->bucketList.lpNext->bucketList.lpPrev = lpCur->bucketList.lpPrev;
						}

						if(lpObj->base.lpSystem == NULL) {
							free((void*)lpCur);
						} else {
							lpObj->base.lpSystem->free(lpObj->base.lpSystem, (void*)lpCur);
						}

						lpObj->dwElementCount = lpObj->dwElementCount - 1;
						return cjsonE_Ok;
					} else {
						/* Overwrite the entry after releasing the old one */
						cjsonReleaseValue(lpCur->lpValue);
						lpCur->lpValue = lpValue;

						/* We don't need the new descriptor, we keep the old one ... */
						if(lpObj->base.lpSystem == NULL) {
							free((void*)lpNewEnt);
						} else {
							lpObj->base.lpSystem->free(lpObj->base.lpSystem, (void*)lpNewEnt);
						}

						return cjsonE_Ok;
					}
				}
			}
			lpCurPrev = lpCur;
			lpCur = lpCur->bucketList.lpNext;
		}

		/* No matching entry exists, append ourself to the list */
		lpNewEnt->bucketList.lpPrev = lpCurPrev;
		lpCurPrev->bucketList.lpNext = lpNewEnt;
		lpObj->dwElementCount = lpObj->dwElementCount + 1;
		return cjsonE_Ok;
	}
}
enum cjsonError cjsonObject_Get(
	const struct cjsonValue* lpObject,
	const char* lpKey,
	unsigned long int dwKeyLength,
	struct cjsonValue** lpValueOut
) {
	unsigned long int idx;
	const struct cjsonObject_BucketEntry* lpCur;

	const struct cjsonObject* lpObj = (const struct cjsonObject*)lpObject;

	if(lpValueOut == NULL) { return cjsonE_InvalidParam; }
	(*lpValueOut) = NULL;

	/* Determine hashtable index */
	idx = getHashIndex(lpObj, lpKey, dwKeyLength);

	if(lpObj->buckets[idx] == NULL) { return cjsonE_IndexOutOfBounds; }

	lpCur = lpObj->buckets[idx];
	while(lpCur != NULL) {
		if(lpCur->dwKeyLength == dwKeyLength) {
			if(memcmp(lpCur->bKey, lpKey, dwKeyLength) == 0) {
				/* Matching key */
				(*lpValueOut) = lpCur->lpValue;
				return cjsonE_Ok;
			}
		}
		lpCur = lpCur->bucketList.lpNext;
	}

	return cjsonE_IndexOutOfBounds;
}
enum cjsonError cjsonObject_HasKey(
	const struct cjsonValue* lpObject,
	const char* lpKey,
	unsigned long int dwKeyLength
) {
	unsigned long int idx;
	const struct cjsonObject_BucketEntry* lpCur;

	const struct cjsonObject* lpObj = (const struct cjsonObject*)lpObject;

	/* Determine hashtable index */
	idx = getHashIndex(lpObj, lpKey, dwKeyLength);

	if(lpObj->buckets[idx] == NULL) { return cjsonE_IndexOutOfBounds; }

	lpCur = lpObj->buckets[idx];
	while(lpCur != NULL) {
		if(lpCur->dwKeyLength == dwKeyLength) {
			if(memcmp(lpCur->bKey, lpKey, dwKeyLength) == 0) {
				/* Matching key */
				return cjsonE_Ok;
			}
		}
		lpCur = lpCur->bucketList.lpNext;
	}

	return cjsonE_IndexOutOfBounds;
}

enum cjsonError cjsonObject_Iterate(
	const struct cjsonValue* lpObject,
	cjsonObject_Iterate_Callback callback,
	void* callbackFreeParam
) {
	enum cjsonError e;
	unsigned long int idxBucket;
	struct cjsonObject_BucketEntry* lpCur;
	struct cjsonObject* lpObj = (struct cjsonObject*)lpObject;

	if(lpObject == NULL) { return cjsonE_InvalidParam; }
	if(lpObject->type != cjsonObject) { return cjsonE_InvalidParam; }
	if(callback == NULL) { return cjsonE_InvalidParam; }

	for(idxBucket = 0; idxBucket < lpObj->dwBucketCount; idxBucket = idxBucket + 1) {
		lpCur = lpObj->buckets[idxBucket];
		while(lpCur != NULL) {
			e = callback(lpCur->bKey, lpCur->dwKeyLength, lpCur->lpValue, callbackFreeParam);
			/* TODO: Implement mutabiltiy based on return value (i.e. object deletion) */
			if(e != cjsonE_Ok) {
				return e;
			}
			lpCur = lpCur->bucketList.lpNext;
		}
	}
	return cjsonE_Ok;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
