# ANSI C99 JSON serializer / deserializer

This is a small ANSI C99 library that provides an easy interface to deserialize
arbitrary UTF-8 JSON input into an abstract tree as well as methods to
traverse through the tree and to re-serialize such an internal representation
into JSON again.

This library has no external dependencies except the libc. Currently
libc is used for memory management as well as snprintf to print
unsigned long, signed long and double values.

## Reading JSON input

To read JSON input one has to create a parser that will call a given
callback for every decoded JSON document inside the stream. There are
flags that control the behaviour of the parser (they can be combined
with binary or):

* `CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS` enables the parser to allow
  duplicate keys inside JSON Objects
* `CJSON_PARSER_FLAG__STREAMINGMODE` enables streaming mode. In this
  mode the parser is capable of continuing parsing after the first
  document that has been encountered. If this bit is not set any
  data following the first root element is an syntax error

```
static enum cjsonError documentReadyCallback(
    struct cjsonValue* lpDocument,
    void* lpFreeParam
) {
    /* Do whatever you want with the document */
    return cjsonE_Ok;
}
```

```
struct cjsonParser* lpParser;
enum cjsonError e;

e = cjsonParserCreate(
    &lpParser,
    0, /* Or CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS|CJSON_PARSER_FLAG__STREAMINGMODE */
    &documentReadyCallback,
    NULL,
    NULL
);
if(e != cjsonE_Ok) {
    /* Perform error handling */
}
```

After that one has to feed the parser with bytes from the input stream and
after finishing input one has to release the parser again:

```
for(...) { /* Iterate over input bytes */
    e = cjsonParserProcessByte(lpParser, nextInputByte);
    if(e != cjsonE_Ok) {
        /* Do error handling */
    }
}
cjsonParserRelease(lpParser);
```

## Writing JSON output

One can write any JSON element (`struct cjsonValue`) into an output stream
by using the serializer. The serializer supports optional pretty-print which
adds formatting that makes human reading of the output simpler. When automatic
parsing is desired one should not set the `CJSON_SERIALIZER__FLAG__PRETTYPRINT`
flag.

An callback to write output byte(s) is also required:

```
static enum cjsonError outputWriterRoutine(
   char* lpData,
   unsigned long int dwBytesToWrite,

   unsigned long int *lpBytesWrittenOut,

   void* lpFreeParam
) {
   /* Do whatever is required to write the bytes */

   /* We always have to update lpBytesWrittenOut! */
   (*lpBytesWrittenOut) = ...;
}
```

```
struct cjsonSerializer* lpSerializer;
enum cjsonError e;

e = cjsonSerializer_Create(
   &lpSerializer,
   &outputWriterRoutine,
   NULL, /* Parameter arbitrarily useable by outputWriterRoutine */
   CJSON_SERIALIZER__FLAG__PRETTYPRINT,
   NULL /* One can supply an custom memory allocation interface here */
);
```

Then any JSON value can be serialized and the serializer can be
released:

```
struct cjsonValue* value;

e = cjsonSerializer_Serialize(lpSerializer, value);
/* Do error handling */

e = cjsonSerializer_Release(lpSerializer);
/* Do error handling */
```

## Traversing an JSON tree and accessing values

To determine the type of an `struct jsonValue*` one can use the following
macros:

* `cjsonIsNull` returns true if the value has been constant `null`
* `cjsonIsTrue` returns true if the value has been constant `true`
* `cjsonIsFalse` returns true if the value has been constant `false`
* `cjsonIsNumeric` returns true if the value has been any of the numeric types
* `cjsonIsULong` value is represented as unsigned long
* `cjsonIsSLong` value is represented as signed long
* `cjsonIsDouble` value is represented as double precision floating point value
* `cjsonIsString` the JSON element is an arbitrary string
* `cjsonIsArray` an ordered list of JSON elements
* `cjsonIsObject` an unordered key-value store

All values can be released via

```
void cjsonReleaseValue(
    struct cjsonValue* lpValue
);
```

### Accessing ordered lists (arrays)

```
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
```

### Accessing key-value stores (objects)

```
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
```

### Accessing numeric types

```
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
```

### Accessing strings

```
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
```

### Accessing constants

```
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
```
