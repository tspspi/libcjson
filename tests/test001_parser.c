#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/cjson.h"

#ifdef __cplusplus
	extern "C" {
#endif

static void debugDumpJson(unsigned long int dwNestingLevel, struct cjsonValue* lpValue);

static enum cjsonError debugDumpJson_ObjIterator(
	char* lpKey,
	unsigned long int dwKeyLength,
	struct cjsonValue* lpValue,
	void* lpFreeParam
) {
	unsigned long int dwNestingLevel = (unsigned long int)lpFreeParam;
	unsigned long int i;

	for(i = 0; i < dwNestingLevel+1; i=i+1) { printf("\t"); }
	printf("\"%.*s\" : ", (int)dwKeyLength, lpKey);
	debugDumpJson(dwNestingLevel+1, lpValue);
	printf(",\n");

	return cjsonE_Ok;
}
static enum cjsonError debugDumpJson_ArrayIterator(
	unsigned long int index,
	struct cjsonValue* lpValue,
	void* lpFreeParam
) {
	unsigned long int dwNestingLevel = (unsigned long int)lpFreeParam;
	unsigned long int i;
	for(i = 0; i < dwNestingLevel+1; i=i+1) { printf("\t"); }
	debugDumpJson(dwNestingLevel+1, lpValue);
	printf(",\n");

	return cjsonE_Ok;
}

static void debugDumpJson(unsigned long int dwNestingLevel, struct cjsonValue* lpValue) {
	unsigned long int i;

	if(lpValue == NULL) { return; }

	switch(lpValue->type) {
		case cjsonObject:
			printf("{\n");
			cjsonObject_Iterate(lpValue, &debugDumpJson_ObjIterator, (void*)dwNestingLevel);
			for(i = 0; i < dwNestingLevel; i=i+1) { printf("\t"); }
			printf("}");
			break;
		case cjsonArray:
			printf("[\n");
			cjsonArray_Iterate(lpValue, &debugDumpJson_ArrayIterator, (void*)dwNestingLevel);
			for(i = 0; i < dwNestingLevel; i=i+1) { printf("\t"); }
			printf("]");
			break;
		case cjsonString:
			printf("\"%.*s\"", (int)((struct cjsonString*)lpValue)->dwStrlen, ((struct cjsonString*)lpValue)->bData); break;
		case cjsonNumber_UnsignedLong:
			printf("%lu", ((struct cjsonNumber*)lpValue)->value.ulong); break;
		case cjsonNumber_SignedLong:
			printf("%ld", ((struct cjsonNumber*)lpValue)->value.slong); break;
		case cjsonNumber_Double:
			printf("%lf", ((struct cjsonNumber*)lpValue)->value.dbl); break;
		case cjsonTrue:
			printf("true"); break;
		case cjsonFalse:
			printf("false"); break;
		case cjsonNull:
			printf("null"); break;
		default:
			printf("ERROR: UNKNOWN TYPE IDENTIFIER %u", lpValue->type); break;
	}
	return;
}

static char* jsonTest_String1 = "\"string1\"";
static char* jsonTest_Const_True = "true";
static char* jsonTest_Const_False = "false";
static char* jsonTest_Const_Null = "null";

static char* jsonTest_Num1 = "123 ";
static char* jsonTest_Num2 = "123.456 ";
static char* jsonTest_Num3 = "1e5 ";
static char* jsonTest_Num4 = "123.456e-3 ";
static char* jsonTest_Num5 = "-123.456e-3 ";
static char* jsonTest_Num6 = "-123.456e+3 ";
static char* jsonTest_Num7 = "123.456e+3 ";

static char* jsonTest_Array1 = "[ \
   \"line1\",	\
   \"line2\",	\
   \"line3\",	\
   \"line4\",	\
   \"line5\"	\
]";
static char* jsonTest_Array2 = "[ \
   \"line1\",	\
   [			\
   	\"line2\",	\
    \"line3\", 	\
	123,		\
	true,		\
	null		\
   ],           \
   \"line4\",	\
   \"line5\"	\
]";

static char* jsonTest_Object1 = "{				\
	\"key1\" : \"stringvalue1\",				\
	\"key2\" : 123,								\
	\"key3\" : true,							\
	\"nested\": {								\
		\"a\" : \"A\",								\
		\"b\" : \"B\",								\
		\"c\" : \"C\",								\
		\"d\" : \"D\"								\
	},											\
	\"lastkey\" : null							\
}";

static enum cjsonError jsonDocumentReadyCallback(
	struct cjsonValue* lpDocument,
	void* lpFreeParam
) {
	printf("\n%s:%u Received document:\n", __FILE__, __LINE__);
	debugDumpJson(0, lpDocument);
	cjsonReleaseValue(lpDocument);
	return cjsonE_Ok;
}

static int runParseTest(char* lpTest) {
	struct cjsonParser* lpParser;
	enum cjsonError e;
	unsigned long int i;

	e = cjsonParserCreate(&lpParser, CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS, &jsonDocumentReadyCallback, NULL, NULL);
	if(e != cjsonE_Ok) { printf("%s:%u: Failed (code %u)\n", __FILE__, __LINE__, e); return 0; }
	printf("%s:%u Parser created ...\n", __FILE__, __LINE__);
	for(i = 0; i < strlen(lpTest); i=i+1) {
		e = cjsonParserProcessByte(lpParser, lpTest[i]);
		if(e != cjsonE_Ok) {
			printf("%s:%u Failed %u\n", __FILE__, __LINE__, e);
			cjsonParserRelease(lpParser);
			return 0;
		}
	}
	printf("\n%s:%u Success\n\n", __FILE__, __LINE__);
	cjsonParserRelease(lpParser);
	return 1;
}

int main(int argc, char* argv[]) {
	struct cjsonParser* lpParser;
	enum cjsonError e;
	FILE* fHandle;
	int i;

	runParseTest(jsonTest_String1);
	runParseTest(jsonTest_Const_True);
	runParseTest(jsonTest_Const_False);
	runParseTest(jsonTest_Const_Null);

	runParseTest(jsonTest_Num1);
	runParseTest(jsonTest_Num2);
	runParseTest(jsonTest_Num3);
	runParseTest(jsonTest_Num4);
	runParseTest(jsonTest_Num5);
	runParseTest(jsonTest_Num6);
	runParseTest(jsonTest_Num7);

	runParseTest(jsonTest_Array1);
	runParseTest(jsonTest_Array2);

	runParseTest(jsonTest_Object1);



	printf("%s:%u Trying to read testfile1.json\n", __FILE__, __LINE__);
	fHandle = fopen("testfile1.json", "r");
	if(!fHandle) {
		printf("%s:%u Failed to open testfile1.json\n", __FILE__, __LINE__);
	} else {
		e = cjsonParserCreate(&lpParser, CJSON_PARSER_FLAG__STREAMINGMODE|CJSON_PARSER_FLAG__ALLOWDUPLICATEKEYS, &jsonDocumentReadyCallback, NULL, NULL);
		if(e != cjsonE_Ok) { printf("%s:%u: Failed (code %u)\n", __FILE__, __LINE__, e); return 0; }
		printf("%s:%u Parser created ...\n", __FILE__, __LINE__);

		while((i = fgetc(fHandle)) != EOF) {
			e = cjsonParserProcessByte(lpParser, (char)i);
			if(e != cjsonE_Ok) {
				printf("%s:%u Failed %u\n", __FILE__, __LINE__, e);
				cjsonParserRelease(lpParser);
				break;
			}
		}
		printf("\n%s:%u Success (if no failure has shown up)\n\n", __FILE__, __LINE__);
	}

	return 0;
}

#ifdef __cplusplus
	} /* extern "C" { */
#endif
