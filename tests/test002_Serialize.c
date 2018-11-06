#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cjson.h"

#ifdef __cplusplus
	extern "C" {
#endif

static enum cjsonError outputWriterRoutine(
	char*											lpData,
	unsigned long int								dwBytesToWrite,
	unsigned long int								*lpBytesWrittenOut,

	void*											lpFreeParam
) {
	unsigned long int dwBytesToGo;

	(*lpBytesWrittenOut) = 0;
	for(dwBytesToGo = 0; dwBytesToGo < dwBytesToWrite; dwBytesToGo = dwBytesToGo + 1) {
		printf("%c", lpData[dwBytesToGo]);
		(*lpBytesWrittenOut) = (*lpBytesWrittenOut) + 1;
	}

	return cjsonE_Ok;
}

static char* strTestString1 = "Test string simple\r\nThis also includes / forward slashed and some \\ backwards slashes";

static char* strJSONTestArraysStringsConsts = "[	\
	\"test1\",										\
	true,											\
	false,											\
	null,											\
	\"test2\",										\
	[ ],											\
	[												\
		\"nested\",									\
		\"array\"									\
	],												\
	{												\
		\"a\" : \"b\",								\
		\"whateverkey\" : true,						\
		\"nested\" : {								\
			\"object\" : \"present\"				\
		}											\
	}												\
]";

static char* strJSONTestArraysStringConst2 = "{		\
	\"Element1\" : \"abc\",							\
	\"Element2\" : 123,								\
	\"Element3\" : true,							\
	\"Element4\" : null,							\
	\"Element5\" : -123,							\
	\"Element6\" : -123.456,						\
	\"Unicode a\" : \"\\u00C4\",					\
	\"Unicode b\" : \"\\u20AC\"						\
}";


static int debugTestSerializer(struct cjsonValue* value) {
	enum cjsonError e;
	struct cjsonSerializer* lpSerializer;

	e = cjsonSerializer_Create(&lpSerializer, &outputWriterRoutine, NULL, CJSON_SERIALIZER__FLAG__PRETTYPRINT, NULL); /* CJSON_SERIALIZER__FLAG__PRETTYPRINT */
	if(e != cjsonE_Ok) {
		printf("%s:%u Failed to create serializer (code %u)\n", __FILE__, __LINE__, e);
		return 1;
	}

	/* Serialize */
	e = cjsonSerializer_Serialize(lpSerializer, value);
	printf("\n");
	if(e != cjsonE_Ok) { printf("%s:%u Failed to serialize (code %u)\n", __FILE__, __LINE__, e); }

	e = cjsonSerializer_Release(lpSerializer);
	if(e != cjsonE_Ok) {
		printf("%s:%u Failed to destroy serializer (code %u)\n", __FILE__, __LINE__, e);
		return 1;
	}

	cjsonReleaseValue(value);
	printf("%s:%u Done successfully\n", __FILE__, __LINE__);
	return 0;
}
static struct cjsonValue* lpLastDeserialized = NULL;
static enum cjsonError debugTestSerializerDeserialize_DocumentReadyCallback(
	struct cjsonValue* lpDocument,
	void* lpFreeParam
) {
	lpLastDeserialized = lpDocument;
	return cjsonE_Ok;
}

static int debugTestSerializerDeserialize(char* lpTestJSON) {
	struct cjsonParser* lpParser;
	enum cjsonError e;
	unsigned long int i;

	e = cjsonParserCreate(&lpParser, CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS, &debugTestSerializerDeserialize_DocumentReadyCallback, NULL, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u: Failed (code %u)\n", __FILE__, __LINE__, e); return 1; }

	for(i = 0; i < strlen(lpTestJSON); i=i+1) {
		e = cjsonParserProcessByte(lpParser, lpTestJSON[i]);
		if(e != cjsonE_Ok) {
			printf("%s:%u Failed %u\n", __FILE__, __LINE__, e);
			cjsonParserRelease(lpParser);
			return 0;
		}
	}
	cjsonParserRelease(lpParser);

	printf("%s:%uParsing ...\n", __FILE__, __LINE__);
	return debugTestSerializer(lpLastDeserialized);
}


int main(int argc, char* argv[]) {
	enum cjsonError e;
	struct cjsonValue* lpTestString;
	struct cjsonValue* lpTestValue;

	printf("%s:%u Testing string serialization:\n-----------------------------\n\n", __FILE__, __LINE__);

	/* Create a simple string node */
	e = cjsonString_Create(&lpTestString, strTestString1, strlen(strTestString1), NULL);
	if(e != cjsonE_Ok) { printf("%s:%u Failed to create node (code %u)\n", __FILE__, __LINE__, e); return 1; }
	debugTestSerializer(lpTestString);
	printf("\n\n");

	printf("%s:%u Testing constant serialization:\n-------------------------------\n\n", __FILE__, __LINE__);
	e = cjsonTrue_Create(&lpTestValue, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u Failed to create node (code %u)\n", __FILE__, __LINE__, e); return 1; }
	debugTestSerializer(lpTestValue);
	printf("\n\n");

	printf("%s:%u Testing constant serialization:\n-------------------------------\n\n", __FILE__, __LINE__);
	e = cjsonFalse_Create(&lpTestValue, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u Failed to create node (code %u)\n", __FILE__, __LINE__, e); return 1; }
	debugTestSerializer(lpTestValue);
	printf("\n\n");

	printf("%s:%u Testing constant serialization:\n-------------------------------\n\n", __FILE__, __LINE__);
	e = cjsonNull_Create(&lpTestValue, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u Failed to create node (code %u)\n", __FILE__, __LINE__, e); return 1; }
	debugTestSerializer(lpTestValue);
	printf("\n\n");

	printf("%s:%u Testing array serialization:\n-------------------------------\n\n", __FILE__, __LINE__);
	e = cjsonNull_Create(&lpTestValue, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u Failed to create node (code %u)\n", __FILE__, __LINE__, e); return 1; }
	debugTestSerializerDeserialize(strJSONTestArraysStringsConsts);
	cjsonReleaseValue(lpTestValue);
	printf("\n\n");

	printf("%s:%u Testing object serialization:\n-------------------------------\n\n", __FILE__, __LINE__);
	e = cjsonNull_Create(&lpTestValue, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u Failed to create node (code %u)\n", __FILE__, __LINE__, e); return 1; }
	debugTestSerializerDeserialize(strJSONTestArraysStringConst2);
	cjsonReleaseValue(lpTestValue);
	printf("\n\n");


	return 0;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
