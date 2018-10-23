#ifndef __is_included__098c74de_0994_4613_bfd7_3d7939e07c89
#define __is_included__098c74de_0994_4613_bfd7_3d7939e07c89 1

#ifndef CJSON_BLOCKSIZE_STRING
	#define	CJSON_BLOCKSIZE_STRING 256
#endif
#ifndef CJSON_BLOCKSIZE_OBJECT
	#define CJSON_BLOCKSIZE_OBJECT 64
#endif

#include <stdint.h>

#ifdef __cplusplus
	extern "C" {
#endif

enum cjsonElementType {
	cjsonUnknown,

	cjsonObject,
	cjsonArray,
	cjsonString,
	cjsonNumber_UnsignedLong,
	cjsonNumber_SignedLong,
	cjsonNumber_Double,
	cjsonTrue,
	cjsonFalse,
	cjsonNull
};

enum cjsonError {
	cjsonE_Ok									= 0,

	cjsonE_InvalidParam							= 1,
	cjsonE_OutOfMemory							= 2,
	cjsonE_AlreadyFinished						= 3,
	cjsonE_EncodingError						= 4,
	cjsonE_IndexOutOfBounds						= 5,
	cjsonE_Finished								= 6,
	cjsonE_OkRedeliver							= 7,
	cjsonE_InvalidState							= 8,

	cjsonE_ImplementationError,
};

/*
	We do interact with the system via an additional system interface.
	This allows the usage of pooled memory allocators.
*/
struct cjsonSystemAPI; /* Forward declaration */

typedef enum cjsonError (*cjsonSystemAPI_Alloc)(
	struct cjsonSystemAPI*						lpSelf,
	unsigned long int							dwSize,
	void**										lpDataOut
);
typedef enum cjsonError (*cjsonSystemAPI_Free)(
	struct cjsonSystemAPI*						lpSelf,
	void*										lpObject
);
struct cjsonSystemAPI {
	cjsonSystemAPI_Alloc						alloc;
	cjsonSystemAPI_Free							free;
};

/*
	cjsonValue is the base type of all objects.
*/
struct cjsonValue {
	enum cjsonElementType				type;					/* Type can be used to cast to the correct type */
	struct cjsonSystemAPI* 				lpSystem;				/* Reference to system is required to correctly release that object */
};

/*
	All numeric datatypes are stored inside cjsonNumber.
	Depending on the type enumeration different types
	can be stored inside the object.
*/
struct cjsonNumber {
	struct cjsonValue					base;

	union {
		unsigned long int 				ulong;
		signed long int					slong;
		double							dbl;
	} value;
};

/*
	Strings are immutable and are embedded in
	their structure. They should be UTF-8 but
	are handeled opaque by this library.
*/
struct cjsonString {
	struct cjsonValue					base;

	unsigned long int					dwStrlen;
	char								bData[];
};

/*
	An array is an ordered unnamed list of elements.
	To implement growable access  this is implemented
	as an linked list of arrays with fixed size but
	variable usage. This allows more lightweight
	insertion and deletion than a normal array at
	only a smaller tradeof for random access. Iteration
	is nearly as performant as on normal arrays.
*/
struct cjsonArray_Page {
	struct {
		struct cjsonArray_Page*			lpNext;
		struct cjsonArray_Page*			lpPrev;
	} pageList;

	unsigned long int					dwUsedEntries;
	struct cjsonValue*					entries[];
};
struct cjsonArray {
	struct cjsonValue					base;

	unsigned long int 					dwPageSize;
	unsigned long int					dwElementCount;
	struct {
		struct cjsonArray_Page*			lpFirstPage;
		struct cjsonArray_Page*			lpLastPage;
	} pageList;
};

/*
	Objects are key/value stores. Each key has to
	be unique. They are implemented as hashmaps
	with a fixed bucket count (at object creation).
*/
struct cjsonObject_BucketEntry {
	struct {
		struct cjsonObject_BucketEntry	*lpNext;
		struct cjsonObject_BucketEntry	*lpPrev;
	} bucketList;

	struct cjsonValue*					lpValue;
	unsigned long int					dwKeyLength;
	char								bKey[];
};
struct cjsonObject {
	struct cjsonValue					base;

	/*
		HashMap implementation
	*/
	unsigned long int					dwElementCount;
	unsigned long int					dwBucketCount;
	unsigned long int					dwBucketShiftBytes;
	struct cjsonObject_BucketEntry*		buckets[];
};


/*
	Access and manipulation definitions and functions
*/
#define cjsonIsNull(_x) 	(_x->type == cjsonNull)
#define cjsonIsTrue(_x) 	(_x->type == cjsonTrue)
#define cjsonIsFalse(_x) 	(_x->type == cjsonFalse)
#define cjsonIsNumeric(_x) 	((_x->type == cjsonNumber_Double) || (_x->type == cjsonNumber_SignedLong) || (_x->type == cjsonNumber_UnsignedLong))
#define cjsonIsULong(_x) 	(_x->type == cjsonNumber_UnsignedLong)
#define cjsonIsSLong(_x) 	(_x->type == cjsonNumber_SignedLong)
#define cjsonIsDouble(_x) 	(_x->type == cjsonNumber_Double)
#define cjsonIsString(_x) 	(_x->type == cjsonString);
#define cjsonIsArray(_x) 	(_x->type == cjsonArray);
#define cjsonIsObject(_x) 	(_x->type == cjsonObject);

void cjsonReleaseValue(
	struct cjsonValue* lpValue
);

/*
	JSON Array access
*/
enum cjsonError cjsonArray_Create(
	struct cjsonValue**	lpArrayOut,
	struct cjsonSystemAPI* lpSystem
);
unsigned long int cjsonArray_Length(
	const struct cjsonValue* lpArray
);
enum cjsonError cjsonArray_Get(
	const struct cjsonValue* lpArray,
	unsigned long int 	idx,
	struct cjsonValue** lpOut
);
enum cjsonError cjsonArray_Set(
	struct cjsonValue* 	lpArray,
	unsigned long int 	idx,
	struct cjsonValue* 	lpIn
);
enum cjsonError cjsonArray_Push(
	struct cjsonValue* 	lpArray,
	struct cjsonValue* 	lpValue
);
typedef enum cjsonError (*cjsonArray_Iterate_Callback)(
	unsigned long int index,
	struct cjsonValue* lpValue,
	void* lpFreeParam
);
enum cjsonError cjsonArray_Iterate(
	struct cjsonValue* lpValue,
	cjsonArray_Iterate_Callback callback,
	void* lpFreeParam
);

/*
	JSON object access
*/
enum cjsonError cjsonObject_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
);
enum cjsonError cjsonObject_Set(
	struct cjsonValue* lpObject,
	const char* lpKey,
	unsigned long int dwKeyLength,
	struct cjsonValue* lpValue
);
enum cjsonError cjsonObject_Get(
	const struct cjsonValue* lpObject,
	const char* lpKey,
	unsigned long int dwKeyLength,
	struct cjsonValue** lpValueOut
);
enum cjsonError cjsonObject_HasKey(
	const struct cjsonValue* lpObject,
	const char* lpKey,
	unsigned long int dwKeyLength
);
typedef enum cjsonError (*cjsonObject_Iterate_Callback)(
	char* lpKey,
	unsigned long int dwKeyLength,
	struct cjsonValue* lpValue,
	void* lpFreeParam
);
enum cjsonError cjsonObject_Iterate(
	const struct cjsonValue* lpObject,
	cjsonObject_Iterate_Callback callback,
	void* callbackFreeParam
);

/*
	Number access
*/
enum cjsonError cjsonNumber_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
);
unsigned long int cjsonObject_GetAsULong(
	struct cjsonValue* lpValue
);
signed long int cjsonObject_GetAsSLong(
	struct cjsonValue* lpValue
);
double cjsonObject_GetAsDouble(
	struct cjsonValue* lpValue
);
enum cjsonError cjsonNumber_SetULong(
	struct cjsonValue* lpValue,
	unsigned long int value
);
enum cjsonError cjsonNumber_SetSLong(
	struct cjsonValue* lpValue,
	signed long int value
);
enum cjsonError cjsonNumber_SetDouble(
	struct cjsonValue* lpValue,
	double value
);

/*
	String access
*/
enum cjsonError cjsonString_Create(
	struct cjsonValue** lpStringOut,
	const char* lpData,
	unsigned long int dwDataLength,
	struct cjsonSystemAPI* lpSystem
);
char* cjsonString_Get(
	struct cjsonValue* lpValue
);
unsigned long int cjsonString_Strlen(
	struct cjsonValue* lpValue
);

/*
	Special objects
*/
enum cjsonError cjsonTrue_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
);
enum cjsonError cjsonFalse_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
);
enum cjsonError cjsonNull_Create(
	struct cjsonValue** lpOut,
	struct cjsonSystemAPI* lpSystem
);
enum cjsonError cjsonBoolean_Set(
	struct cjsonValue* lpValue,
	int value
);

/*
	Parser (Deserializer)
*/
#define CJSON_PARSER_FLAG__STREAMINGMODE		0x00000001	/* Streaming mode allows multiple "root" objects but requires an document callback */
/* Note that duplicate keys are NOT SUPPORTED CURRENTLY! */
#define CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS	0x00000002	/* Silently ignore duplicate keys inside objects and always use the last one. If not set raise an parser error on duplicate keys */

#define CJSON_PARSER_FLAG__INTERNAL_DONE		0x80000000	/* Used to signal that we are not in streaming mode and have already finished */

struct cjsonParser_BufferChain_Entry {
	struct cjsonParser_BufferChain_Entry*		lpNext;
	unsigned long int							dwUsedBytes;
	char										bData[];
};
struct cjsonParser_BufferChain {
	struct cjsonParser_BufferChain_Entry*		lpFirst;
	struct cjsonParser_BufferChain_Entry*		lpLast;
	unsigned long int							dwPageSize;
	unsigned long int							dwBytesUsed;
};

enum cjsonParser_StateStackType {
	cjsonParser_StateStackType__Object,
	cjsonParser_StateStackType__Array,
	cjsonParser_StateStackType__Number,
	cjsonParser_StateStackType__String,
	cjsonParser_StateStackType__Constant
};

struct cjsonParser_StateStackElement {
	enum cjsonParser_StateStackType				type;
	struct cjsonParser_StateStackElement*		lpNext;
};
struct cjsonParser_StateStackElement_Constant {
	struct cjsonParser_StateStackElement		base;
	unsigned long int							dwMatchedBytes;
	enum cjsonElementType						elmType;
};

enum cjsonParser_StateStackElement_String_State {
	cjsonParser_StateStackElement_String_State__Normal,
	cjsonParser_StateStackElement_String_State__Escaped,
	cjsonParser_StateStackElement_String_State__UTF16Codepoint
};
struct cjsonParser_StateStackElement_String {
	struct cjsonParser_StateStackElement		base;
	struct cjsonParser_BufferChain				buf;
	enum cjsonParser_StateStackElement_String_State state;
	unsigned long int							dwUBytes;
	unsigned long int							dwUCodepoint;
};

enum cjsonParser_StateStackElement_Array_State {
	cjsonParser_StateStackElement_Array_State_NoComma,
	cjsonParser_StateStackElement_Array_State_GotComma
};
struct cjsonParser_StateStackElement_Array {
	struct cjsonParser_StateStackElement			base;
	struct cjsonValue*								lpArrayObject;
	enum cjsonParser_StateStackElement_Array_State	state;
};

enum cjsonParser_StateStackElement_Object_State {
	cjsonParser_StateStackElement_Object_State__ExpectKey,		/* If currently NO objects have been read this can directly go into closed state */
	cjsonParser_StateStackElement_Object_State__ReadKey,		/* Colon is expected */
	cjsonParser_StateStackElement_Object_State__ExpectObject,	/* The object has not been started but the colon has been seen */
	cjsonParser_StateStackElement_Object_State__ReadObject		/* Either comma or end of object are expected */
};
struct cjsonParser_StateStackElement_Object {
	struct cjsonParser_StateStackElement			base;
	struct cjsonValue*								lpObjectObject;
	unsigned long int								dwReadObjects;
	enum cjsonParser_StateStackElement_Object_State	state;

	char*											lpCurrentKey;
	unsigned long int								dwCurrentKeyLength;
};


enum cjsonParser_StateStackElement_Number_State {
	cjsonParser_StateStackElement_Number_State__FirstSymbol,
	cjsonParser_StateStackElement_Number_State__IntegerDigits,
	cjsonParser_StateStackElement_Number_State__FractionalDigitsFirst,
	cjsonParser_StateStackElement_Number_State__FractionalDigits,
	cjsonParser_StateStackElement_Number_State__ExponentFirst,
	cjsonParser_StateStackElement_Number_State__ExponentFirstAfterSign,
	cjsonParser_StateStackElement_Number_State__Exponent
};
struct cjsonParser_StateStackElement_Number {
	struct cjsonParser_StateStackElement			base;

	union {
		unsigned long int								ulong;
		signed long int									slong;
		double											dDouble;
	} currentValue;

	double												dCurrentMultiplier;
	signed long int										dwCurrentExponent;

	signed long int										dwSignBase;
	signed long int										dwSignExponent;

	enum cjsonElementType								numberType;
	enum cjsonParser_StateStackElement_Number_State 	state;
};

typedef enum cjsonError (*lpfnCJSONCallback_DocumentReady)(
	struct cjsonValue* lpDocument,
	void* lpFreeParam
);

struct cjsonParser {
	/*
		Parser state stack.
	*/
	struct cjsonParser_StateStackElement*		lpStateStack;
	unsigned long int							dwStateStackDepth;
	struct cjsonValue*							lpChildResult;		/* The result of the last child parsed (if any). Also used for the last document (!) */

	/* Configuration */
	uint32_t									dwFlags;
	struct cjsonSystemAPI*						lpSystem;
	lpfnCJSONCallback_DocumentReady 			callbackDocumentReady;
	void* 										callbackDocumentReadyFreeParam;
};


enum cjsonError cjsonParserCreate(
	struct cjsonParser** lpOut,
	uint32_t dwFlags,

	lpfnCJSONCallback_DocumentReady callbackDocumentRead,
	void* callbackDocumentReadyFreeParam,

	struct cjsonSystemAPI* lpSystem
);
enum cjsonError cjsonParserProcessByte(
	struct cjsonParser* lpParser,
	char bByte
);
enum cjsonError cjsonParserRelease(
	struct cjsonParser* lpParser
);

/*
	Serializer
*/
#define CJSON_SERIALIZER__FLAG__PRETTYPRINT			0x00000001
enum cjsonSerializer_StackEntryType {
	cjsonSerializer_StackEntryType__Array,
	cjsonSerializer_StackEntryType__Constant,
	cjsonSerializer_StackEntryType__Number,
	cjsonSerializer_StackEntryType__Object,
	cjsonSerializer_StackEntryType__String
};
struct cjsonSerializer_StackEntry {
	enum cjsonSerializer_StackEntryType				type;
	struct cjsonSerializer_StackEntry*				lpNext;
};

/* String serializer */
enum cjsonSerializer_String_State {
	cjsonSerializer_String_State__LeadingQuote,
	cjsonSerializer_String_State__Normal,
	cjsonSerializer_String_State__Escaped,
	cjsonSerializer_String_State__Unicode
};
struct cjsonSerializer_String {
	struct cjsonSerializer_StackEntry				base;
	struct cjsonValue*								lpStringObject;

	enum cjsonSerializer_String_State				state;

	unsigned long int								dwCurrentIndex;		/* Index into the UTF-8 bytestream inside cjsoNValue object */
	uint8_t											bUnicodeBytes[4];
	unsigned long int								dwUnicodeBytesWritten;
};

struct cjsonSerializer_Constant {
	struct cjsonSerializer_StackEntry				base;
	enum cjsonElementType							constantType;
	unsigned long int								dwBytesWritten;
};

struct cjsonSerializer_Array {
	struct cjsonSerializer_StackEntry				base;
	struct cjsonArray*								lpArray;
	unsigned long int								dwBytesWritten; 	/* used for header and trailer */
	unsigned long int								dwCurrentIndex;
	unsigned long int								dwWrittenIndent;
	unsigned long int								dwWrittenPast;
};

enum cjsonSerializer_Object_State {
	cjsonSerializer_Object_State__Header,
	cjsonSerializer_Object_State__Key,
	cjsonSerializer_Object_State__Value,
	cjsonSerializer_Object_State__Trailer
};
struct cjsonSerializer_Object {
	struct cjsonSerializer_StackEntry				base;
	struct cjsonObject*								lpObject;
	struct cjsonValue*								lpTempString;

	unsigned long int								dwBytesWritten; /* Used for header, trailer, colon, comma and indent */
	enum cjsonSerializer_Object_State				state;

	unsigned long int								dwCurrentBucket;
	struct cjsonObject_BucketEntry*					lpCurrentBucketEntry;
};

typedef enum cjsonError (*cjsonSerializer_Callback_WriteBytes)(
	char*											lpData,
	unsigned long int								dwBytesToWrite,
	unsigned long int								*lpBytesWrittenOut,

	void*											lpFreeParam
);

struct cjsonSerializer_Number {
	struct cjsonSerializer_StackEntry				base;
	unsigned long int								dwStringLen;
	unsigned long int								dwWritten;
	char											bString[];
};

struct cjsonSerializer {
	struct cjsonSerializer_StackEntry*				lpTopOfStack;
	struct cjsonSystemAPI*							lpSystem;
	unsigned long int								dwStateStackDepth;
	uint32_t										dwFlags;

	cjsonSerializer_Callback_WriteBytes				callbackWriteBytes;
	void*											callbackWriteBytesParam;
};

enum cjsonError cjsonSerializer_Create(
	struct cjsonSerializer** lpOut,
	cjsonSerializer_Callback_WriteBytes callback,
	void* callbackFreeParam,
	uint32_t dwFlags,
	struct cjsonSystemAPI* lpSystem
);
enum cjsonError cjsonSerializer_Serialize(
	struct cjsonSerializer* lpSerializer,
	struct cjsonValue* lpValue
);
enum cjsonError cjsonSerializer_Continue(
	struct cjsonSerializer* lpSerializer
);
enum cjsonError cjsonSerializer_Release(
	struct cjsonSerializer* lpSerializer
);

#ifdef __cplusplus
	} /* extern "C" { */
#endif

#endif /* #ifndef __is_included__098c74de_0994_4613_bfd7_3d7939e07c89 */
