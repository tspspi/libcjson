#include "../include/cjson.h"
#include <stdlib.h>

#ifndef CJSON_BLOCKSIZE_ARRAY
	#define CJSON_BLOCKSIZE_ARRAY 64
#endif

#ifdef __cplusplus
	extern "C" {
#endif

const unsigned long int cjsonArray_BlockSize = CJSON_BLOCKSIZE_ARRAY;

enum cjsonError cjsonArray_Create(
	struct cjsonValue**	lpArrayOut,
	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;

	if(lpArrayOut == NULL) { return cjsonE_InvalidParam; }
	(*lpArrayOut) = NULL;

	if(lpSystem == NULL) {
		(*lpArrayOut) = (struct cjsonValue*)malloc(sizeof(struct cjsonArray));
		if((*lpArrayOut) == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonArray), (void**)lpArrayOut);
		if(e != cjsonE_Ok) { return e; }
	}

	((struct cjsonArray*)(*lpArrayOut))->base.type = cjsonArray;
	((struct cjsonArray*)(*lpArrayOut))->base.lpSystem = NULL;
	((struct cjsonArray*)(*lpArrayOut))->dwPageSize = CJSON_BLOCKSIZE_ARRAY;
	((struct cjsonArray*)(*lpArrayOut))->dwElementCount = 0;
	((struct cjsonArray*)(*lpArrayOut))->pageList.lpFirstPage = NULL;
	((struct cjsonArray*)(*lpArrayOut))->pageList.lpLastPage = NULL;

	return cjsonE_Ok;
}
unsigned long int cjsonArray_Length(
	const struct cjsonValue* lpArray
) {
	if(lpArray == NULL) { return cjsonE_InvalidParam; }
	if(lpArray->type != cjsonArray) { return cjsonE_InvalidParam; }

	return ((struct cjsonArray*)lpArray)->dwElementCount;
}
enum cjsonError cjsonArray_Get(
	const struct cjsonValue* lpArray,
	unsigned long int 	idx,
	struct cjsonValue** lpOut
) {
	const struct cjsonArray* lpThis = (struct cjsonArray*)lpArray;
	const struct cjsonArray_Page* lpCurPage;
	unsigned long int dwCurBase;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if(lpArray == NULL) { return cjsonE_InvalidParam; }
	if(lpArray->type != cjsonArray) { return cjsonE_InvalidParam; }

	if(idx >= lpThis->dwElementCount) { return cjsonE_IndexOutOfBounds; }

	/* Locate the page this entry is located at */
	lpCurPage = lpThis->pageList.lpFirstPage;
	if(lpCurPage == NULL) { return cjsonE_ImplementationError; }
	dwCurBase = 0;

	while(idx >= dwCurBase+lpCurPage->dwUsedEntries) {
		dwCurBase = dwCurBase + lpCurPage->dwUsedEntries;
		lpCurPage = lpCurPage->pageList.lpNext;
		if(lpCurPage == NULL) { return cjsonE_ImplementationError; }
	}

	(*lpOut) = lpCurPage->entries[idx-dwCurBase];
	return cjsonE_Ok;
}
enum cjsonError cjsonArray_Set(
	struct cjsonValue* 	lpArray,
	unsigned long int 	idx,
	struct cjsonValue* 	lpIn
) {
	struct cjsonArray* lpThis = (struct cjsonArray*)lpArray;
	struct cjsonArray_Page* lpCurPage;
	struct cjsonValue* lpOld;
	unsigned long int dwCurBase;

	if(lpArray == NULL) { return cjsonE_InvalidParam; }
	if(lpArray->type != cjsonArray) { return cjsonE_InvalidParam; }

	if(idx >= lpThis->dwElementCount) { return cjsonE_IndexOutOfBounds; }

	/* Locate the page this entry is located at */
	lpCurPage = lpThis->pageList.lpFirstPage;
	if(lpCurPage == NULL) { return cjsonE_ImplementationError; }
	dwCurBase = 0;

	while(idx >= dwCurBase+lpCurPage->dwUsedEntries) {
		dwCurBase = dwCurBase + lpCurPage->dwUsedEntries;
		lpCurPage = lpCurPage->pageList.lpNext;
		if(lpCurPage == NULL) { return cjsonE_ImplementationError; }
	}

	lpOld = lpCurPage->entries[idx-dwCurBase];
	lpCurPage->entries[idx-dwCurBase] = lpIn;
	if(lpOld != NULL) { cjsonReleaseValue(lpOld); } /* Release old entry */

	return cjsonE_Ok;
}
enum cjsonError cjsonArray_Push(
	struct cjsonValue* 	lpArray,
	struct cjsonValue* 	lpValue
) {
	enum cjsonError e;
	unsigned long int i;
	struct cjsonArray* lpThis = (struct cjsonArray*)lpArray;
	struct cjsonArray_Page* lpNewPage;

	if(lpArray == NULL) { return cjsonE_InvalidParam; }
	if(lpArray->type != cjsonArray) { return cjsonE_InvalidParam; }

	if(lpThis->pageList.lpLastPage == NULL) {
		/* We insert the first page ... */
		if(lpThis->base.lpSystem == NULL) {
			lpNewPage = (struct cjsonArray_Page*)malloc(sizeof(struct cjsonArray_Page)+sizeof(struct cjsonValue*)*lpThis->dwPageSize);
			if(lpNewPage == NULL) { return cjsonE_OutOfMemory; }
		} else {
			e = lpThis->base.lpSystem->alloc(lpThis->base.lpSystem, sizeof(struct cjsonArray_Page)+sizeof(struct cjsonValue*)*lpThis->dwPageSize, (void**)(&lpNewPage));
			if(e != cjsonE_Ok) { return e; }
		}
		lpNewPage->pageList.lpNext = NULL;
		lpNewPage->pageList.lpPrev = NULL;
		lpNewPage->dwUsedEntries = 1;
		for(i = 1; i < lpThis->dwPageSize; i=i+1) { lpNewPage->entries[i] = NULL; }
		lpNewPage->entries[0] = lpValue;

		lpThis->pageList.lpFirstPage = lpNewPage;
		lpThis->pageList.lpLastPage = lpNewPage;
		lpThis->dwElementCount = lpThis->dwElementCount + 1;
		return cjsonE_Ok;
	}

	if(lpThis->pageList.lpLastPage->dwUsedEntries < lpThis->dwPageSize) {
		/* There are entries available in the last element */
		lpThis->pageList.lpLastPage->entries[lpThis->pageList.lpLastPage->dwUsedEntries] = lpValue;
		lpThis->pageList.lpLastPage->dwUsedEntries = lpThis->pageList.lpLastPage->dwUsedEntries + 1;
		lpThis->dwElementCount = lpThis->dwElementCount + 1;
		return cjsonE_Ok;
	} else {
		/* We need a new page that will be appended */
		if(lpThis->base.lpSystem == NULL) {
			lpNewPage = (struct cjsonArray_Page*)malloc(sizeof(struct cjsonArray_Page)+sizeof(struct cjsonValue*)*lpThis->dwPageSize);
			if(lpNewPage == NULL) { return cjsonE_OutOfMemory; }
		} else {
			e = lpThis->base.lpSystem->alloc(lpThis->base.lpSystem, sizeof(struct cjsonArray_Page)+sizeof(struct cjsonValue*)*lpThis->dwPageSize, (void**)(&lpNewPage));
			if(e != cjsonE_Ok) { return e; }
		}
		lpNewPage->pageList.lpNext = NULL;
		lpNewPage->pageList.lpPrev = lpThis->pageList.lpLastPage;
		lpNewPage->dwUsedEntries = 1;
		for(i = 1; i < lpThis->dwPageSize; i=i+1) { lpNewPage->entries[i] = NULL; }
		lpNewPage->entries[0] = lpValue;

		lpThis->pageList.lpLastPage->pageList.lpNext = lpNewPage;
		lpThis->pageList.lpLastPage = lpNewPage;
		lpThis->dwElementCount = lpThis->dwElementCount + 1;
		return cjsonE_Ok;
	}
}

enum cjsonError cjsonArray_Iterate(
	struct cjsonValue* lpArray,
	cjsonArray_Iterate_Callback callback,
	void* lpFreeParam
) {
	enum cjsonError e;
	unsigned long int i;
	unsigned long int dwIdx;
	struct cjsonArray* lpThis = (struct cjsonArray*)lpArray;
	struct cjsonArray_Page* lpCurrentPage;

	if(lpArray == NULL) { return cjsonE_InvalidParam; }
	if(lpArray->type != cjsonArray) { return cjsonE_InvalidParam; }
	if(callback == NULL) { return cjsonE_InvalidParam; }

	lpCurrentPage = lpThis->pageList.lpFirstPage;
	dwIdx = 0;
	while(lpCurrentPage != NULL) {
		for(i = 0; i < lpCurrentPage->dwUsedEntries; i=i+1) {
			e = callback(dwIdx, lpCurrentPage->entries[i], lpFreeParam);
			if(e != cjsonE_Ok) {
				/* TODO: Implement mutability - i.e. allow deletion */
				return e;
			}
			dwIdx = dwIdx + 1;
		}
		lpCurrentPage = lpCurrentPage->pageList.lpNext;
	}
	return cjsonE_Ok;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
