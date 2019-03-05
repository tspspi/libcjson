#include "../include/cjson.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
	extern "C" {
#endif

#ifndef CJSON_PARSER_BLOCKSIZE_STRING
	#define CJSON_PARSER_BLOCKSIZE_STRING 512
#endif

/*
	Some constants
*/
static const char* strTrue 		= "true";
static const char* strFalse 	= "false";
static const char* strNull 		= "null";

/*
	And forward declarations for this local file
*/
struct cjsonParser;

static inline enum cjsonError cjsonParserMallocHelper(struct cjsonParser* lpParser, unsigned long int dwSize, void** lpOut);
static inline void cjsonParserFreeHelper(struct cjsonParser* lpParser, void* lpArea);

static enum cjsonError cjsonParser_Constant_Push(struct cjsonParser* lpParser, enum cjsonElementType eType);
static enum cjsonError cjsonParser_String_Push(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Object_Push(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Array_Push(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Number_Push(struct cjsonParser* lpParser);

static enum cjsonError cjsonParser_Universe_ProcessByte(struct cjsonParser* lpParser, char bByte);
static enum cjsonError cjsonParser_Constant_ProcessByte(struct cjsonParser* lpParser, char bByte);
static enum cjsonError cjsonParser_String_ProcessByte(struct cjsonParser* lpParser, char bByte);
static enum cjsonError cjsonParser_Object_ProcessByte(struct cjsonParser* lpParser, char bByte);
static enum cjsonError cjsonParser_Array_ProcessByte(struct cjsonParser* lpParser, char bByte);
static enum cjsonError cjsonParser_Number_ProcessByte(struct cjsonParser* lpParser, char bByte);

static enum cjsonError cjsonParser_Universe_PopNotify(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Constant_PopNotify(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_String_PopNotify(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Object_PopNotify(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Array_PopNotify(struct cjsonParser* lpParser);
static enum cjsonError cjsonParser_Number_PopNotify(struct cjsonParser* lpParser);

/*
	Buffer chain helpers
*/
static inline void cjsonParser_BufferChain_Init(
	struct cjsonParser* lpParser,
	struct cjsonParser_BufferChain* lpChain,
	unsigned long int dwPageSize
) {
	lpChain->lpFirst = NULL;
	lpChain->lpLast = NULL;
	lpChain->dwPageSize = dwPageSize;
	lpChain->dwBytesUsed = 0;
	return;
}
static inline void cjsonParser_BufferChain_Release(
	struct cjsonParser* lpParser,
	struct cjsonParser_BufferChain* lpChain
) {
	struct cjsonParser_BufferChain_Entry* lpCur;
	struct cjsonParser_BufferChain_Entry* lpNext;

	lpCur = lpChain->lpFirst;
	while(lpCur != NULL) {
		lpNext = lpCur->lpNext;
		cjsonParserFreeHelper(lpParser, (void*)lpCur);
		lpCur = lpNext;
	}
	lpChain->lpFirst = NULL;
	lpChain->lpLast = NULL;
}
static enum cjsonError cjsonParser_BufferChain_PushByte(
	struct cjsonParser* lpParser,
	struct cjsonParser_BufferChain* lpChain,
	char bData
) {
	enum cjsonError e;
	struct cjsonParser_BufferChain_Entry* lpPage;

	if(lpChain->lpFirst == NULL) {
		/* This is the first page */
		e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_BufferChain_Entry)+lpChain->dwPageSize, (void**)(&lpPage));
		if(e != cjsonE_Ok) { return e; }

		lpPage->lpNext = NULL;
		lpPage->dwUsedBytes = 1;
		lpPage->bData[0] = bData;
		lpChain->lpFirst = lpPage;
		lpChain->lpLast = lpPage;
		lpChain->dwBytesUsed = lpChain->dwBytesUsed + 1;
		return cjsonE_Ok;
	} else {
		/* Maybe we can append */
		if(lpChain->lpLast->dwUsedBytes < lpChain->dwPageSize) {
			lpChain->lpLast->bData[lpChain->lpLast->dwUsedBytes] = bData;
			lpChain->lpLast->dwUsedBytes = lpChain->lpLast->dwUsedBytes + 1;
			lpChain->dwBytesUsed = lpChain->dwBytesUsed + 1;
			return cjsonE_Ok;
		}

		/* We have to expand with a new page */
		e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_BufferChain_Entry)+lpChain->dwPageSize, (void**)(&lpPage));
		if(e != cjsonE_Ok) { return e; }

		lpPage->lpNext = NULL;
		lpPage->dwUsedBytes = 1;
		lpPage->bData[0] = bData;
		lpChain->dwBytesUsed = lpChain->dwBytesUsed + 1;

		lpChain->lpLast->lpNext = lpPage;
		lpChain->lpLast = lpPage;
		return cjsonE_Ok;
	}
}
static inline unsigned long int cjsonParser_BufferChain_Length(
	struct cjsonParser* lpParser,
	struct cjsonParser_BufferChain* lpChain
) {
	return lpChain->dwBytesUsed;
}
static inline void cjsonParser_BufferChain_MemcpyOut(
	struct cjsonParser* lpParser,
	struct cjsonParser_BufferChain* lpChain,
	char* lpDestBuffer,
	unsigned long int dwBytesToCopy
) {
	unsigned long int dwBytesToGo;
	unsigned long int dwBase;
	struct cjsonParser_BufferChain_Entry* lpCur;

	if((dwBytesToGo = dwBytesToCopy) > lpChain->dwBytesUsed) { return; }
	dwBase = 0;
	lpCur = lpChain->lpFirst;

	while(dwBytesToGo > 0) {
		if(lpCur->dwUsedBytes < dwBytesToGo) {
			memcpy(&(lpDestBuffer[dwBase]), lpCur->bData, lpCur->dwUsedBytes);
			dwBytesToGo = dwBytesToGo - lpCur->dwUsedBytes;
			dwBase = dwBase + lpCur->dwUsedBytes;
		} else {
			memcpy(&(lpDestBuffer[dwBase]), lpCur->bData, dwBytesToGo);
			dwBase = dwBase + dwBytesToGo;
			dwBytesToGo = 0;
		}
		lpCur = lpCur->lpNext;
	}
	return;
}

/*
	State stack pop helper. Note that popping REQUIRES the previous
	state to have released ALL resources except the struct
	cjsonParser_StateStackElement on top of the stack.
*/
static enum cjsonError cjsonParser_StateStackPop(
	struct cjsonParser* lpParser
) {
	struct cjsonParser_StateStackElement* lpElm;

	if(lpParser->lpStateStack == NULL) { return cjsonE_InvalidParam; }

	lpElm = lpParser->lpStateStack;
	lpParser->lpStateStack = lpElm->lpNext;

	cjsonParserFreeHelper(lpParser, (void*)lpElm);

	if(lpParser->lpStateStack == NULL) { 			return cjsonParser_Universe_PopNotify(lpParser); }

	switch(lpParser->lpStateStack->type) {
		case cjsonParser_StateStackType__Object:	return cjsonParser_Object_PopNotify(lpParser);
		case cjsonParser_StateStackType__Array:		return cjsonParser_Array_PopNotify(lpParser);
		case cjsonParser_StateStackType__Number:	return cjsonParser_Number_PopNotify(lpParser);
		case cjsonParser_StateStackType__String:	return cjsonParser_String_PopNotify(lpParser);
		case cjsonParser_StateStackType__Constant:	return cjsonParser_Constant_PopNotify(lpParser);
		default:									return cjsonE_ImplementationError;
	}
}

/*
	Some memory management helpers
*/

static inline enum cjsonError cjsonParserMallocHelper(
	struct cjsonParser* lpParser,
	unsigned long int dwSize,
	void** lpOut
) {
	if(lpParser->lpSystem == NULL) {
		(*lpOut) = malloc(dwSize);
		if((*lpOut) == NULL) { return cjsonE_OutOfMemory; }
		return cjsonE_Ok;
	} else {
		return lpParser->lpSystem->alloc(lpParser->lpSystem, dwSize, lpOut);
	}
}

static inline void cjsonParserFreeHelper(
	struct cjsonParser* lpParser,
	void* lpArea
) {
	if(lpParser->lpSystem == NULL) {
		free(lpArea);
	} else {
		lpParser->lpSystem->free(lpParser->lpSystem, lpArea);
	}
}

/*
	Parser for undefined state (JSONDocument "universe")
*/
static enum cjsonError cjsonParser_Universe_ProcessByte(
	struct cjsonParser* lpParser,
	char bByte
) {
	enum cjsonError e;
	switch(bByte) {
		/* We may start an object */
		case '{':
			return cjsonParser_Object_Push(lpParser);

		/* We may start an array */
		case '[':
			return cjsonParser_Array_Push(lpParser);

		/* We may start an string */
		case '"':
			return cjsonParser_String_Push(lpParser);

		/* We may start true, false or null */
		case 't':
			return cjsonParser_Constant_Push(lpParser, cjsonTrue);

		case 'f':
			return cjsonParser_Constant_Push(lpParser, cjsonFalse);

		case 'n':
			return cjsonParser_Constant_Push(lpParser, cjsonNull);

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '-':
			e = cjsonParser_Number_Push(lpParser);
			if(e != cjsonE_Ok) { return e; }
			return cjsonE_OkRedeliver; /* We require the digit or sign inside the number parser ... */

		/* Whitespace as specified */
		case 0x09:
		case 0x0A:
		case 0x0D:
		case 0x20:
			return cjsonE_Ok;

		/* Anything else is an error */
		default:
			return cjsonE_EncodingError;
	}
}
static enum cjsonError cjsonParser_Universe_PopNotify(
	struct cjsonParser* lpParser
) {
	enum cjsonError e;

	/*
		We have returned from our child ... we
		now notify the registered callback (if any)
	*/
	if((lpParser->callbackDocumentReady != NULL) && (lpParser->lpChildResult != NULL)) {
		e = lpParser->callbackDocumentReady(lpParser->lpChildResult, lpParser->callbackDocumentReadyFreeParam);
		lpParser->lpChildResult = NULL;
		if((lpParser->dwFlags & CJSON_PARSER_FLAG__STREAMINGMODE) == 0) {
			lpParser->dwFlags = lpParser->dwFlags | CJSON_PARSER_FLAG__INTERNAL_DONE;
		}
	} else {
		e = cjsonE_Finished;
	}
	return e;
}

/*
	Parser for constants (true, false, null)
*/
static enum cjsonError cjsonParser_Constant_Push(
	struct cjsonParser* lpParser,
	enum cjsonElementType eType
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Constant* lpNewConst;

	e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_StateStackElement_Constant), (void**)(&lpNewConst));
	if(e != cjsonE_Ok) { return e; }

	switch(eType) {
		case cjsonTrue:		lpNewConst->elmType = eType; break;
		case cjsonFalse:	lpNewConst->elmType = eType; break;
		case cjsonNull:		lpNewConst->elmType = eType; break;
		default:			cjsonParserFreeHelper(lpParser, (void*)lpNewConst); return cjsonE_InvalidParam;
	}

	 lpNewConst->base.type = cjsonParser_StateStackType__Constant;
	 lpNewConst->base.lpNext = lpParser->lpStateStack;
	 lpNewConst->dwMatchedBytes = 1; /* The first byte is already consumed by the callee */

	 /* Push to top of state stack */
	 lpParser->lpStateStack = (struct cjsonParser_StateStackElement*)lpNewConst;
	 return cjsonE_Ok;
}
static enum cjsonError cjsonParser_Constant_ProcessByte(
	struct cjsonParser* lpParser,
	char bByte
) {
	enum cjsonError e;

	struct cjsonParser_StateStackElement_Constant* lpConst = (struct cjsonParser_StateStackElement_Constant*)(lpParser->lpStateStack);
	switch(lpConst->elmType) {
		case cjsonTrue:
			if(bByte != strTrue[lpConst->dwMatchedBytes]) { return cjsonE_EncodingError; }
			lpConst->dwMatchedBytes = lpConst->dwMatchedBytes + 1;
			if(lpConst->dwMatchedBytes != strlen(strTrue)) { return cjsonE_Ok; }

			/* We have fully read that element, create the descriptor in lpChildResult and pop ... */
			if(lpParser->lpChildResult != NULL) { cjsonReleaseValue(lpParser->lpChildResult); lpParser->lpChildResult = NULL; }
			e = cjsonTrue_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
			if(e != cjsonE_Ok) { return e; }
			return cjsonParser_StateStackPop(lpParser);

		case cjsonFalse:
			if(bByte != strFalse[lpConst->dwMatchedBytes]) { return cjsonE_EncodingError; }
			lpConst->dwMatchedBytes = lpConst->dwMatchedBytes + 1;
			if(lpConst->dwMatchedBytes != strlen(strFalse)) { return cjsonE_Ok; }

			/* We have fully read that element, create the descriptor in lpChildResult and pop ... */
			if(lpParser->lpChildResult != NULL) { cjsonReleaseValue(lpParser->lpChildResult); lpParser->lpChildResult = NULL; }
			e = cjsonFalse_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
			if(e != cjsonE_Ok) { return e; }
			return cjsonParser_StateStackPop(lpParser);

		case cjsonNull:
			if(bByte != strNull[lpConst->dwMatchedBytes]) { return cjsonE_EncodingError; }
			lpConst->dwMatchedBytes = lpConst->dwMatchedBytes + 1;
			if(lpConst->dwMatchedBytes != strlen(strNull)) { return cjsonE_Ok; }

			/* We have fully read that element, create the descriptor in lpChildResult and pop ... */
			if(lpParser->lpChildResult != NULL) { cjsonReleaseValue(lpParser->lpChildResult); lpParser->lpChildResult = NULL; }
			e = cjsonNull_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
			if(e != cjsonE_Ok) { return e; }
			return cjsonParser_StateStackPop(lpParser);

		default:
			return cjsonE_ImplementationError;
	}
}
static enum cjsonError cjsonParser_Constant_PopNotify(
	struct cjsonParser* lpParser
) {
	/*
		We can never be called after a child has been processed,
		we are child-free!
	*/
	return cjsonE_ImplementationError;
}

/*
	Parser for (quoted) strings
*/
static enum cjsonError cjsonParser_String_Push(
	struct cjsonParser* lpParser
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_String* lpNewStr;

	e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_StateStackElement_String), (void**)(&lpNewStr));
	if(e != cjsonE_Ok) { return e; }

	lpNewStr->base.type = cjsonParser_StateStackType__String;
	lpNewStr->base.lpNext = lpParser->lpStateStack;
	lpNewStr->state = cjsonParser_StateStackElement_String_State__Normal;

	cjsonParser_BufferChain_Init(lpParser, &(lpNewStr->buf), CJSON_PARSER_BLOCKSIZE_STRING);

	lpParser->lpStateStack = (struct cjsonParser_StateStackElement*)lpNewStr;
	return cjsonE_Ok;
}
static enum cjsonError cjsonParser_String_ProcessByte(
	struct cjsonParser* lpParser,
	char bData
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_String* lpStr;
	struct cjsonString* lpValue;

	lpStr = (struct cjsonParser_StateStackElement_String*)(lpParser->lpStateStack);

	if(lpStr->state == cjsonParser_StateStackElement_String_State__Normal) {
		if(bData == '\\') {
			lpStr->state = cjsonParser_StateStackElement_String_State__Escaped;
			return cjsonE_Ok;
		}
		if(bData != '"') {
			return cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), bData);
		}

		/* End of string ... */
		e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonString)+cjsonParser_BufferChain_Length(lpParser, &(lpStr->buf)), (void**)(&lpValue));
		if(e != cjsonE_Ok) { return e; }

		cjsonParser_BufferChain_MemcpyOut(lpParser, &(lpStr->buf), lpValue->bData, cjsonParser_BufferChain_Length(lpParser, &(lpStr->buf)));
		lpValue->dwStrlen = cjsonParser_BufferChain_Length(lpParser, &(lpStr->buf));
		lpValue->base.lpSystem = lpParser->lpSystem;
		lpValue->base.type = cjsonString;

		if(lpParser->lpChildResult != NULL) { cjsonReleaseValue(lpParser->lpChildResult); lpParser->lpChildResult = NULL; }
		lpParser->lpChildResult = (struct cjsonValue*)lpValue;

		cjsonParser_BufferChain_Release(lpParser, &(lpStr->buf));
		return cjsonParser_StateStackPop(lpParser);
	} else if(lpStr->state == cjsonParser_StateStackElement_String_State__Escaped) {
		switch(bData) {
			case '\\':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '\\'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case '/':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '/'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case 'b':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '\b'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case 'f':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '\f'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case 'n':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '\n'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case 'r':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '\r'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case 't':		e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), '\t'); lpStr->state = cjsonParser_StateStackElement_String_State__Normal; return e;
			case 'u':		lpStr->state = cjsonParser_StateStackElement_String_State__UTF16Codepoint; lpStr->dwUBytes = 0; lpStr->dwUCodepoint = 0; return cjsonE_Ok;
			default:		return cjsonE_EncodingError;
		}
	} else if(lpStr->state == cjsonParser_StateStackElement_String_State__UTF16Codepoint) {
		if((bData >= '0') && (bData <= '9')) {
			lpStr->dwUCodepoint = lpStr->dwUCodepoint * 16 + (unsigned long int)(bData - '0');
		} else if((bData >= 'a') && (bData <= 'f')) {
			lpStr->dwUCodepoint = lpStr->dwUCodepoint * 16 + (unsigned long int)(bData - 'a' + 10);
		} else if((bData >= 'A') && (bData <= 'F')) {
			lpStr->dwUCodepoint = lpStr->dwUCodepoint * 16 + (unsigned long int)(bData - 'A' + 10);
		} else {
			return cjsonE_EncodingError;
		}

		if((lpStr->dwUBytes = lpStr->dwUBytes + 1) < 4) {
			return cjsonE_Ok;
		}

		/*
			Finished. Convert codepoint to UTF-8 string and insert into
			our buffer, then continue unescaped.
		*/
		if(lpStr->dwUCodepoint <= 0x7F) {
			/* Single byte encoding */
			e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), (uint8_t)((lpStr->dwUCodepoint) & 0x7F));
			if(e != cjsonE_Ok) { return e; }
		} else if(lpStr->dwUCodepoint <= 0x07FF) {
			/* Two byte encoding */
			e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), (uint8_t)((lpStr->dwUCodepoint >> 6) & 0x1F) | 0xC0);
			if(e != cjsonE_Ok) { return e; }
			e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), (uint8_t)((lpStr->dwUCodepoint) & 0x3F) | 0x80);
			if(e != cjsonE_Ok) { return e; }
		} else {
			/* Three byte encoding */
			e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), (uint8_t)((lpStr->dwUCodepoint >> 12) & 0x0F) | 0xE0);
			if(e != cjsonE_Ok) { return e; }
			e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), (uint8_t)((lpStr->dwUCodepoint >> 6) & 0x3F) | 0x80);
			if(e != cjsonE_Ok) { return e; }
			e = cjsonParser_BufferChain_PushByte(lpParser, &(lpStr->buf), (uint8_t)((lpStr->dwUCodepoint) & 0x3F) | 0x80);
			if(e != cjsonE_Ok) { return e; }
		}

		lpStr->state = cjsonParser_StateStackElement_String_State__Normal;
		return cjsonE_Ok;
	} else {
		return cjsonE_ImplementationError;
	}
}
static enum cjsonError cjsonParser_String_PopNotify(
	struct cjsonParser* lpParser
) {
	/*
		We can never be called after a child has been processed,
		we are child-free!
	*/
	return cjsonE_ImplementationError;
}

/*
	Parser for number
*/
static enum cjsonError cjsonParser_Number_Push(
	struct cjsonParser* lpParser
) {
	struct cjsonParser_StateStackElement_Number* lpNew;
	enum cjsonError e;

	e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_StateStackElement_Number), (void**)(&lpNew));
	if(e != cjsonE_Ok) { return e; }

	lpNew->base.type = cjsonParser_StateStackType__Number;
	lpNew->base.lpNext = lpParser->lpStateStack;
	lpNew->currentValue.ulong = 0;
	lpNew->dCurrentMultiplier = 1;
	lpNew->dwCurrentExponent = 0;
	lpNew->dwSignBase = 1.0;
	lpNew->dwSignExponent = 1.0;
	lpNew->numberType = cjsonNumber_UnsignedLong;
	lpNew->state = cjsonParser_StateStackElement_Number_State__FirstSymbol;

	lpParser->lpStateStack = (struct cjsonParser_StateStackElement*)lpNew;
	return cjsonE_Ok;
}
static enum cjsonError cjsonParser_Number_ProcessByte(
	struct cjsonParser* lpParser,
	char bData
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Number* lpState;
	double dTemp;
	lpState = (struct cjsonParser_StateStackElement_Number*)lpParser->lpStateStack;

	switch(lpState->state) {
		case cjsonParser_StateStackElement_Number_State__FirstSymbol:
			/*
				The first symbol can be either
					'-' followed by 0 or digit(s)
					0
					1-9
			*/
			if((bData >= '0') && (bData <= '9')) {
				/* Digit ... */
				lpState->state = cjsonParser_StateStackElement_Number_State__IntegerDigits;
				lpState->currentValue.ulong = (unsigned long int)(bData - '0');
			} else if(bData == '-') {
				lpState->dwSignBase = -1;
				lpState->state = cjsonParser_StateStackElement_Number_State__IntegerDigits;
			} else {
				return cjsonE_EncodingError;
			}
			return cjsonE_Ok;
		case cjsonParser_StateStackElement_Number_State__IntegerDigits:
			/*
				Can be followed by
					Integer digits -> Integer Digits
					Decimal dot -> Fractional part
					e or E -> Exponent
					Anything Else -> Done
			*/
			if((bData >= '0') && (bData <= '9')) {
				/* Digit */
				if(lpState->numberType == cjsonNumber_UnsignedLong) {
					if(lpState->currentValue.ulong < (ULONG_MAX/10)) {
						lpState->currentValue.ulong = (lpState->currentValue.ulong * 10) + ((unsigned long int)(bData - '0'));
					} else {
						lpState->numberType = cjsonNumber_Double;
						lpState->currentValue.dDouble = ((double)(lpState->currentValue.ulong) * 10.0) + (double)(bData - '0');
					}
				} else {
					lpState->currentValue.dDouble = (lpState->currentValue.dDouble * 10.0) + (double)(bData - '0');
				}
				return cjsonE_Ok;
			} else if(bData == '.') {
				/* Switch to fractional part ... */
				lpState->state = cjsonParser_StateStackElement_Number_State__FractionalDigitsFirst;
				if(lpState->numberType == cjsonNumber_UnsignedLong) {
					lpState->currentValue.dDouble = (double)(lpState->currentValue.ulong);
					lpState->numberType = cjsonNumber_Double;
				}
				return cjsonE_Ok;
			} else if((bData == 'e') || (bData == 'E')) {
				/* Switch to exponential part */
				lpState->state = cjsonParser_StateStackElement_Number_State__ExponentFirst;
				if(lpState->numberType == cjsonNumber_UnsignedLong) {
					lpState->currentValue.dDouble = (double)(lpState->currentValue.ulong);
					lpState->numberType = cjsonNumber_Double;
				}
				return cjsonE_Ok;
			} else {
				/* We have finished either an signed or unsigned long ... */
				if(lpState->numberType != cjsonNumber_Double) {
					if(lpState->dwSignBase < 0) {
						/* Create an signed long and pop ... */
						e = cjsonNumber_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
						if(e != cjsonE_Ok) { return e; }
						cjsonNumber_SetSLong(lpParser->lpChildResult, (signed long int)(lpState->currentValue.ulong)*-1);
					} else {
						/* Create an unsigned long and pop */
						e = cjsonNumber_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
						if(e != cjsonE_Ok) { return e; }
						cjsonNumber_SetULong(lpParser->lpChildResult, lpState->currentValue.ulong);
					}
				} else {
					/* Create an double with sign and base */
					e = cjsonNumber_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
					if(e != cjsonE_Ok) { return e; }
					cjsonNumber_SetDouble(lpParser->lpChildResult, lpState->currentValue.dDouble * (double)(lpState->dwSignBase));
				}
				cjsonParser_StateStackPop(lpParser);
				return cjsonE_OkRedeliver; /* Redeliver the symbol to the parent ... */
			}
		case cjsonParser_StateStackElement_Number_State__FractionalDigitsFirst:
			/* At least one digit ... */
			if((bData >= '0') && (bData <= '9')) {
				lpState->state = cjsonParser_StateStackElement_Number_State__FractionalDigits;
				lpState->currentValue.dDouble = lpState->currentValue.dDouble * 10.0 + ((double)(bData - '0'));
				lpState->dCurrentMultiplier = lpState->dCurrentMultiplier / 10.0;
				return cjsonE_Ok;
			} else {
				return cjsonE_EncodingError;
			}
		case cjsonParser_StateStackElement_Number_State__FractionalDigits:
			if((bData >= '0') && (bData <= '9')) {
				lpState->state = cjsonParser_StateStackElement_Number_State__FractionalDigits;
				lpState->currentValue.dDouble = lpState->currentValue.dDouble * 10.0 + ((double)(bData - '0'));
				lpState->dCurrentMultiplier = lpState->dCurrentMultiplier / 10.0;
				return cjsonE_Ok;
			} else if((bData == 'e') || (bData == 'E')) {
				lpState->state = cjsonParser_StateStackElement_Number_State__ExponentFirst;
				return cjsonE_Ok;
			}
			/* Create an double with sign and base */
			e = cjsonNumber_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
			if(e != cjsonE_Ok) { return e; }
			cjsonNumber_SetDouble(lpParser->lpChildResult, lpState->currentValue.dDouble * (double)(lpState->dwSignBase) * lpState->dCurrentMultiplier);
			cjsonParser_StateStackPop(lpParser);
			return cjsonE_OkRedeliver;

		case cjsonParser_StateStackElement_Number_State__ExponentFirst:
			if(bData == '+') {
				lpState->state = cjsonParser_StateStackElement_Number_State__ExponentFirstAfterSign;
				return cjsonE_Ok;
			} else if(bData == '-') {
				lpState->state = cjsonParser_StateStackElement_Number_State__ExponentFirstAfterSign;
				lpState->dwSignExponent = -1.0;
				return cjsonE_Ok;
			} else if((bData >= '0') && (bData <= '9')) {
				lpState->state = cjsonParser_StateStackElement_Number_State__Exponent;
				lpState->dwCurrentExponent = (unsigned long int)(bData - '0');
				return cjsonE_Ok;
			} else {
				return cjsonE_EncodingError;
			}

		case cjsonParser_StateStackElement_Number_State__ExponentFirstAfterSign:
			if((bData >= '0') && (bData <= '9')) {
				lpState->state = cjsonParser_StateStackElement_Number_State__Exponent;
				lpState->dwCurrentExponent = (unsigned long int)(bData - '0');
				return cjsonE_Ok;
			} else {
				return cjsonE_EncodingError;
			}

		case cjsonParser_StateStackElement_Number_State__Exponent:
			if((bData >= '0') && (bData <= '9')) {
				lpState->state = cjsonParser_StateStackElement_Number_State__Exponent;
				lpState->dwCurrentExponent = lpState->dwCurrentExponent + (unsigned long int)(bData - '0');
				return cjsonE_Ok;
			}
			e = cjsonNumber_Create(&(lpParser->lpChildResult), lpParser->lpSystem);
			if(e != cjsonE_Ok) { return e; }
			dTemp = lpState->currentValue.dDouble * (double)(lpState->dwSignBase) * lpState->dCurrentMultiplier;
			if(lpState->dwSignExponent > 0) {
				while(lpState->dwCurrentExponent > 0) { dTemp = dTemp * 10.0; lpState->dwCurrentExponent = lpState->dwCurrentExponent - 1; }
			} else {
				while(lpState->dwCurrentExponent > 0) { dTemp = dTemp / 10.0; lpState->dwCurrentExponent = lpState->dwCurrentExponent - 1; }
			}
			cjsonNumber_SetDouble(lpParser->lpChildResult, dTemp);
			cjsonParser_StateStackPop(lpParser);
			return cjsonE_OkRedeliver;

		default:
			return cjsonE_ImplementationError;
	}
}
static enum cjsonError cjsonParser_Number_PopNotify(
	struct cjsonParser* lpParser
) {
	/* We have no child elements that can return ... */
	return cjsonE_ImplementationError;
}

/*
	Parser for object
*/
static enum cjsonError cjsonParser_Object_Push(
	struct cjsonParser* lpParser
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Object* lpNewObj;

	e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_StateStackElement_Object), (void**)(&lpNewObj));
	if(e != cjsonE_Ok) { return e; }

	e = cjsonObject_Create(&(lpNewObj->lpObjectObject), lpParser->lpSystem);
	if(e != cjsonE_Ok) {
		cjsonParserFreeHelper(lpParser, (void*)lpNewObj);
		return e;
	}

	lpNewObj->base.type = cjsonParser_StateStackType__Object;
	lpNewObj->base.lpNext = lpParser->lpStateStack;
	lpNewObj->state = cjsonParser_StateStackElement_Object_State__ExpectKey;
	lpNewObj->dwReadObjects = 0;

	lpNewObj->lpCurrentKey = NULL;
	lpNewObj->dwCurrentKeyLength = 0;

	lpParser->lpStateStack = (struct cjsonParser_StateStackElement*)lpNewObj;
	return cjsonE_Ok;
}
static enum cjsonError cjsonParser_Object_ProcessByte(
	struct cjsonParser* lpParser,
	char bData
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Object* lpState;
	lpState = (struct cjsonParser_StateStackElement_Object*)(lpParser->lpStateStack);

	if(lpState->state == cjsonParser_StateStackElement_Object_State__ExpectKey) {
		/*
			Only a string or - when no object has been read - an end of object is allowed.
			Whitespace will be skipped.
		*/
		switch(bData) {
			/* We may start an string */
			case '"':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadKey;		return cjsonParser_String_Push(lpParser);

			/* Whitespace as specified */
			case 0x09:		return cjsonE_Ok;
			case 0x0A:		return cjsonE_Ok;
			case 0x0D:		return cjsonE_Ok;
			case 0x20:		return cjsonE_Ok;

			/* End of object */
			case '}':
				if(lpState->dwReadObjects == 0) {
					lpParser->lpChildResult = lpState->lpObjectObject;
					return cjsonParser_StateStackPop(lpParser);
				} else {
					return cjsonE_EncodingError;
				}

			/* Anything else is an error */
			default:		return cjsonE_EncodingError;
		}
	} else if(lpState->state == cjsonParser_StateStackElement_Object_State__ReadKey) {
		/*
			Only a colon is valid after we've read a key ...
			Whitespace will be skipped.
		*/
		switch(bData) {
			/* We may start an string */
			case ':':		lpState->state = cjsonParser_StateStackElement_Object_State__ExpectObject; return cjsonE_Ok;

			/* Whitespace as specified */
			case 0x09:		return cjsonE_Ok;
			case 0x0A:		return cjsonE_Ok;
			case 0x0D:		return cjsonE_Ok;
			case 0x20:		return cjsonE_Ok;

			/* Anything else is an error */
			default:		return cjsonE_EncodingError;
		}
	} else if(lpState->state == cjsonParser_StateStackElement_Object_State__ExpectObject) {
		/*
			Any object is allowed
			Whitespace will be skipped.
		*/
		switch(bData) {
			case '{':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;		return cjsonParser_Object_Push(lpParser);									/* We may start an object */
			case '[':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;		return cjsonParser_Array_Push(lpParser);									/* We may start an array */
			case '"':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;		return cjsonParser_String_Push(lpParser);									/* We may start an string */
			case 't':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;		return cjsonParser_Constant_Push(lpParser, cjsonTrue);						/* We may start true ... */
			case 'f':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;		return cjsonParser_Constant_Push(lpParser, cjsonFalse);						/* ... start "false" ... */
			case 'n':		lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;		return cjsonParser_Constant_Push(lpParser, cjsonNull);						/* ... or start "null" */

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '-':
				lpState->state = cjsonParser_StateStackElement_Object_State__ReadObject;
				e = cjsonParser_Number_Push(lpParser);
				if(e != cjsonE_Ok) { return e; }
				return cjsonE_OkRedeliver; /* We require the digit or sign inside the number parser ... */

			/* Whitespace as specified */
			case 0x09:		return cjsonE_Ok;
			case 0x0A:		return cjsonE_Ok;
			case 0x0D:		return cjsonE_Ok;
			case 0x20:		return cjsonE_Ok;

			/* Anything else is an error */
			default:		return cjsonE_EncodingError;
		}
	} else {
		/*
			We are in "ReadObject" state so either a comma or an end of object is allowed.
			Whitespace will be skipped.
		*/
		switch(bData) {
			case ',':		lpState->state = cjsonParser_StateStackElement_Object_State__ExpectKey; return cjsonE_Ok;
			case '}':		lpParser->lpChildResult = lpState->lpObjectObject; return cjsonParser_StateStackPop(lpParser);

			/* Whitespace as specified */
			case 0x09:		return cjsonE_Ok;
			case 0x0A:		return cjsonE_Ok;
			case 0x0D:		return cjsonE_Ok;
			case 0x20:		return cjsonE_Ok;

			/* Anything else is an error */
			default:		return cjsonE_EncodingError;
		}
	}
}
static enum cjsonError cjsonParser_Object_PopNotify(
	struct cjsonParser* lpParser
) {
	enum cjsonError e;
	/*
		A child has finished ... so we either keep
		a copy of the key (cjsonParser_StateStackElement_Object_State__ReadKey
		state) or fetch the child result and store it into our
		object hashmap (cjsonParser_StateStackElement_Object_State__ReadObject)
	*/
	struct cjsonParser_StateStackElement_Object* lpState;
	lpState = (struct cjsonParser_StateStackElement_Object*)(lpParser->lpStateStack);

	if(lpState->state == cjsonParser_StateStackElement_Object_State__ReadKey) {
		/* The child object is a jsonValue that contains a string. This will be used as key */
		lpState->dwCurrentKeyLength = cjsonString_Strlen(lpParser->lpChildResult);
		e = cjsonParserMallocHelper(lpParser, sizeof(char)*lpState->dwCurrentKeyLength, (void**)(&(lpState->lpCurrentKey)));
		if(e != cjsonE_Ok) { return e; }
		memcpy(lpState->lpCurrentKey, cjsonString_Get(lpParser->lpChildResult), lpState->dwCurrentKeyLength);
		cjsonReleaseValue(lpParser->lpChildResult); lpParser->lpChildResult = NULL;
		return cjsonE_Ok;
	} else if(lpState->state == cjsonParser_StateStackElement_Object_State__ReadObject) {
		/* The child object is anything ... */
		e = cjsonObject_Set(lpState->lpObjectObject, lpState->lpCurrentKey, lpState->dwCurrentKeyLength, lpParser->lpChildResult);
		cjsonParserFreeHelper(lpParser, (void*)(lpState->lpCurrentKey));
		lpState->lpCurrentKey = NULL; lpState->dwCurrentKeyLength = 0;
		if(e != cjsonE_Ok) {
			cjsonReleaseValue(lpParser->lpChildResult);
		}
		lpParser->lpChildResult = NULL;
		return e;
	} else {
		/* No other state tolerates child objects */
		return cjsonE_ImplementationError;
	}
}
/*
	Parser for array
*/
static enum cjsonError cjsonParser_Array_Push(
	struct cjsonParser* lpParser
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Array* lpNew;

	e = cjsonParserMallocHelper(lpParser, sizeof(struct cjsonParser_StateStackElement_Array), (void**)(&lpNew));
	if(e != cjsonE_Ok) { return e; }

	lpNew->base.type = cjsonParser_StateStackType__Array;
	lpNew->base.lpNext = lpParser->lpStateStack;
	lpNew->state = cjsonParser_StateStackElement_Array_State_NoComma;

	e = cjsonArray_Create(&(lpNew->lpArrayObject), lpParser->lpSystem);
	if(e != cjsonE_Ok) {
		cjsonParserFreeHelper(lpParser, (void*)lpNew);
		return e;
	}

	lpParser->lpStateStack = (struct cjsonParser_StateStackElement*)lpNew;
	return cjsonE_Ok;
}
static enum cjsonError cjsonParser_Array_ProcessByte(
	struct cjsonParser* lpParser,
	char bData
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Array* lpStackElm;

	lpStackElm = (struct cjsonParser_StateStackElement_Array*)(lpParser->lpStateStack);

	switch(bData) {
		/* We may start an object */
		case '{':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			return cjsonParser_Object_Push(lpParser);

		/* We may start an array */
		case '[':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			return cjsonParser_Array_Push(lpParser);

		/* We may start an string */
		case '"':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			return cjsonParser_String_Push(lpParser);

		/* We may start true, false or null */
		case 't':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			return cjsonParser_Constant_Push(lpParser, cjsonTrue);

		case 'f':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			return cjsonParser_Constant_Push(lpParser, cjsonFalse);

		case 'n':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			return cjsonParser_Constant_Push(lpParser, cjsonNull);

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '-':
			lpStackElm->state = cjsonParser_StateStackElement_Array_State_NoComma;
			e = cjsonParser_Number_Push(lpParser);
			if(e != cjsonE_Ok) { return e; }
			return cjsonE_OkRedeliver; /* We require the digit or sign inside the number parser ... */

		/* Whitespace as specified */
		case 0x09:
		case 0x0A:
		case 0x0D:
		case 0x20:
			return cjsonE_Ok;

		case ',':
			if((lpStackElm->state == cjsonParser_StateStackElement_Array_State_NoComma) && (cjsonArray_Length(lpStackElm->lpArrayObject) > 0)) {
				lpStackElm->state = cjsonParser_StateStackElement_Array_State_GotComma;
				return cjsonE_Ok;
			} else {
				return cjsonE_EncodingError;
			}

		/* End of array */
		case ']':
			if(lpStackElm->state == cjsonParser_StateStackElement_Array_State_NoComma) {
				lpParser->lpChildResult = lpStackElm->lpArrayObject;
				return cjsonParser_StateStackPop(lpParser);
			} else {
				return cjsonE_EncodingError;
			}

		/* Anything else is an error */
		default:
			return cjsonE_EncodingError;
	}
}
static enum cjsonError cjsonParser_Array_PopNotify(
	struct cjsonParser* lpParser
) {
	enum cjsonError e;
	struct cjsonParser_StateStackElement_Array* lpStackElm;

	/* A chlid has been parsed ... put it into our array */
	lpStackElm = (struct cjsonParser_StateStackElement_Array*)(lpParser->lpStateStack);
	if(lpParser->lpChildResult != NULL) {
		e = cjsonArray_Push(lpStackElm->lpArrayObject, lpParser->lpChildResult);
		lpParser->lpChildResult = NULL;
		return e;
	} else {
		return cjsonE_ImplementationError;
	}
}



enum cjsonError cjsonParserCreate(
	struct cjsonParser** lpOut,
	uint32_t dwFlags,

	lpfnCJSONCallback_DocumentReady callbackDocumentRead,
	void* callbackDocumentReadyFreeParam,

	struct cjsonSystemAPI* lpSystem
) {
	enum cjsonError e;
	struct cjsonParser* lpNew;

	if(lpOut == NULL) { return cjsonE_InvalidParam; }
	(*lpOut) = NULL;

	if((dwFlags & ~(CJSON_PARSER_FLAG__STREAMINGMODE|CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS)) != 0) { return cjsonE_InvalidParam; }
	if(((dwFlags & CJSON_PARSER_FLAG__STREAMINGMODE) != 0) && (callbackDocumentRead == NULL)) { return cjsonE_InvalidParam; }

	if(lpSystem == NULL) {
		lpNew = (struct cjsonParser*)malloc(sizeof(struct cjsonParser));
		if(lpNew == NULL) { return cjsonE_OutOfMemory; }
	} else {
		e = lpSystem->alloc(lpSystem, sizeof(struct cjsonParser), (void**)(&lpNew));
		if(e != cjsonE_Ok) { return cjsonE_OutOfMemory; }
	}

	lpNew->lpStateStack = NULL;
	lpNew->dwStateStackDepth = 0;
	lpNew->lpChildResult = NULL;
	lpNew->dwFlags = dwFlags;
	lpNew->lpSystem = lpSystem;
	lpNew->callbackDocumentReady = callbackDocumentRead;
	lpNew->callbackDocumentReadyFreeParam = callbackDocumentReadyFreeParam;

	(*lpOut) = lpNew;
	return cjsonE_Ok;
}
enum cjsonError cjsonParserProcessByte(
	struct cjsonParser* lpParser,
	char bByte
) {
	enum cjsonError e;

	/* Pass to current stack element - if any; the element MAY return an ok-redeliver (used internally) */
	if((bByte != 0x09) && (bByte != 0x0A) && (bByte != 0x0D) && (bByte != 0x20)) {
		if((lpParser->dwFlags & CJSON_PARSER_FLAG__INTERNAL_DONE) != 0) { return cjsonE_AlreadyFinished; }
	}

	for(;;) {
		if(lpParser->lpStateStack == NULL) {
			e = cjsonParser_Universe_ProcessByte(lpParser, bByte);
		} else {
			switch(lpParser->lpStateStack->type) {
				case cjsonParser_StateStackType__Object:	e = cjsonParser_Object_ProcessByte(lpParser, bByte); break;
				case cjsonParser_StateStackType__Array:		e = cjsonParser_Array_ProcessByte(lpParser, bByte); break;
				case cjsonParser_StateStackType__Number:	e = cjsonParser_Number_ProcessByte(lpParser, bByte); break;
				case cjsonParser_StateStackType__String:	e = cjsonParser_String_ProcessByte(lpParser, bByte); break;
				case cjsonParser_StateStackType__Constant:	e = cjsonParser_Constant_ProcessByte(lpParser, bByte); break;
				default:									return cjsonE_ImplementationError;
			}
		}
		if(e == cjsonE_OkRedeliver) { continue; }
		break;
	}
	return e;
}

enum cjsonError cjsonParserRelease(
	struct cjsonParser* lpParser
) {
	struct cjsonParser_StateStackElement* lpCurrentStack;
	struct cjsonParser_StateStackElement* lpNextStack;
	/*
		If there is nothing to release we always signal
		success ...
	*/
	if(lpParser == NULL) { return cjsonE_Ok; }

	/* Release any dangling result */
	if(lpParser->lpChildResult != NULL) {
		cjsonReleaseValue(lpParser->lpChildResult);
		lpParser->lpChildResult = NULL;
	}

	lpCurrentStack = lpParser->lpStateStack;

	while(lpCurrentStack != NULL) {
		lpNextStack = lpCurrentStack->lpNext;

		/* Some cleanup that's type specific */
		switch(lpCurrentStack->type) {
			case cjsonParser_StateStackType__Object:
				if(((struct cjsonParser_StateStackElement_Object*)lpCurrentStack)->lpObjectObject != NULL) {
					cjsonReleaseValue(((struct cjsonParser_StateStackElement_Object*)lpCurrentStack)->lpObjectObject);
					((struct cjsonParser_StateStackElement_Object*)lpCurrentStack)->lpObjectObject = NULL;
				}
				if(((struct cjsonParser_StateStackElement_Object*)lpCurrentStack)->lpCurrentKey != NULL) {
					cjsonParserFreeHelper(lpParser, (void*)(((struct cjsonParser_StateStackElement_Object*)lpCurrentStack)->lpCurrentKey));
					((struct cjsonParser_StateStackElement_Object*)lpCurrentStack)->lpCurrentKey = NULL;
				}
				break;
			case cjsonParser_StateStackType__Array:
				if(((struct cjsonParser_StateStackElement_Array*)lpCurrentStack)->lpArrayObject != NULL) {
					cjsonReleaseValue(((struct cjsonParser_StateStackElement_Array*)lpCurrentStack)->lpArrayObject);
					((struct cjsonParser_StateStackElement_Array*)lpCurrentStack)->lpArrayObject = NULL;
				}
				break;
			case cjsonParser_StateStackType__Number:
				break; /* No additional cleanup */
			case cjsonParser_StateStackType__String:
				cjsonParser_BufferChain_Release(lpParser, &(((struct cjsonParser_StateStackElement_String*)lpCurrentStack)->buf));
				break;
			case cjsonParser_StateStackType__Constant:
				break; /* No additional cleanup */
			default:
				return cjsonE_ImplementationError;
		}

		/* Release the descriptor */
		cjsonParserFreeHelper(lpParser, (void*)lpCurrentStack);

		/* And iterate ... */
		lpCurrentStack = lpNextStack;
	}
	lpParser->lpStateStack = NULL;

	cjsonParserFreeHelper(lpParser, (void*)lpParser);
	return cjsonE_Ok;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
