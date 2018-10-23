#include "../include/cjson.h"
#include <stdlib.h>

#ifdef __cplusplus
	extern "C" {
#endif

/*
	Release function used for ALL supported values
*/
void cjsonReleaseValue(
	struct cjsonValue* lpValue
) {
	unsigned long int i;
	void* lpPageRelease;
	struct cjsonArray_Page* lpArrayPage;
	struct cjsonObject_BucketEntry* lpBucketEntry;

	if(lpValue == NULL) { return; }

	switch(lpValue->type) {
		case cjsonObject:
			/*
				At the object we have to iterate over all buckets and
				release all contained elements recursively
			*/
			for(i = 0; i < ((struct cjsonObject*)lpValue)->dwBucketCount; i=i+1) {
				lpBucketEntry = ((struct cjsonObject*)lpValue)->buckets[i];
				while(lpBucketEntry != NULL) {
					lpPageRelease = (void*)lpBucketEntry;
					cjsonReleaseValue(lpBucketEntry->lpValue);

					lpBucketEntry = lpBucketEntry->bucketList.lpNext;
					if(lpValue->lpSystem == NULL) { free(lpPageRelease); } else { lpValue->lpSystem->free(lpValue->lpSystem, lpPageRelease); }
				}
			}
			if(lpValue->lpSystem == NULL) { free(lpValue); } else { lpValue->lpSystem->free(lpValue->lpSystem, lpValue); }
			return;

		case cjsonArray:
			/*
				We have to iterate over all pages and release all
				contained elements recursively
			*/
			lpArrayPage = ((struct cjsonArray*)lpValue)->pageList.lpFirstPage;
			while(lpArrayPage != NULL) {
				for(i = 0; i < lpArrayPage->dwUsedEntries; i=i+1) {
					cjsonReleaseValue(lpArrayPage->entries[i]); /* Note that NULL is also correctly accepted as argument, there is no missing check */
				}
				lpPageRelease = (void*)lpArrayPage;
				lpArrayPage = lpArrayPage->pageList.lpNext;

				if(lpValue->lpSystem == NULL) { free(lpPageRelease); } else { lpValue->lpSystem->free(lpValue->lpSystem, lpPageRelease); }
			}
			if(lpValue->lpSystem == NULL) { free(lpValue); } else { lpValue->lpSystem->free(lpValue->lpSystem, lpValue); }
			return;

		case cjsonString:
		case cjsonNumber_UnsignedLong:
		case cjsonNumber_SignedLong:
		case cjsonNumber_Double:
		case cjsonTrue:
		case cjsonFalse:
		case cjsonNull:
			/* This types only consist of the value structrue */
			if(lpValue->lpSystem == NULL) {
				free((void*)lpValue);
			} else {
				lpValue->lpSystem->free(lpValue->lpSystem, (void*)lpValue);
			}
			return;

		default:
			/* This should NEVER happen */
			return;
	}
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
