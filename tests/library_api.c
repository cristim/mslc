#include "mslc/mslc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct LegacyOptions {
	MslcStage stage;
	uint32_t localSizeX, localSizeY, localSizeZ;
	MslcImageSetPolicy imageSetPolicy;
	const char* sourcePath;
	const char* const* includeDirs;
	size_t includeDirCount;
};
#define OFFSET(field) _Static_assert(offsetof(MslcOptions, field) == offsetof(struct LegacyOptions, field), #field)
_Static_assert(sizeof(MslcOptions) == sizeof(struct LegacyOptions), "options ABI size");
OFFSET(stage); OFFSET(localSizeX); OFFSET(localSizeY); OFFSET(localSizeZ);
OFFSET(imageSetPolicy); OFFSET(sourcePath); OFFSET(includeDirs); OFFSET(includeDirCount);

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

static void rejected(const char* source, const MslcOptions* options, const char* diagnostic) {
	uint8_t* bytes = (uint8_t*)1;
	size_t size = 99;
	char* error = (char*)1;
	CHECK(mslc_compile_library(source, source ? strlen(source) : 0, options, &bytes, &size, &error) == 1);
	CHECK(bytes == NULL && size == 0 && error != NULL);
	if (diagnostic) CHECK(strstr(error, diagnostic) != NULL);
	mslc_free(error);
}

static void accepted(const char* source) {
	uint8_t* bytes = (uint8_t*)1;
	size_t size = 99;
	char* error = (char*)1;
	const uint32_t expected[] = {0x00020011, 1, 0x00020011, 5,
		0x00020011, 5347, 0x0003000e, 5348, 1};
	CHECK(mslc_compile_library(source, strlen(source), NULL, &bytes, &size, &error) == 0);
	CHECK(size == 56 && bytes != NULL && error == NULL);
	uint32_t header[5];
	memcpy(header, bytes, sizeof(header));
	CHECK(header[0] == 0x07230203 && header[1] == 0x00010500 && header[2] == 0
		&& header[3] >= 1 && header[4] == 0);
	CHECK(memcmp(bytes + sizeof(header), expected, sizeof(expected)) == 0);
	mslc_free(bytes);
	CHECK(mslc_compile_library(source, strlen(source), NULL, NULL, &size, NULL) == 0 && size == 56);
}

int main(int argc, char** argv) {
	if (argc == 3) {
		FILE* input = fopen(argv[1], "rb");
		CHECK(input != NULL && fseek(input, 0, SEEK_END) == 0);
		const long length = ftell(input);
		CHECK(length >= 0 && fseek(input, 0, SEEK_SET) == 0);
		char* source = malloc((size_t)length + 1);
		CHECK(source != NULL && fread(source, 1, (size_t)length, input) == (size_t)length && fclose(input) == 0);
		MslcOptions options;
		mslc_default_options(&options);
		options.sourcePath = argv[1];
		uint8_t* bytes = NULL;
		size_t size = 0;
		char* error = NULL;
		const int status = mslc_compile_library(source, (size_t)length, &options, &bytes, &size, &error);
		free(source);
		if (status) { fprintf(stderr, "%s\n", error ? error : "allocation failure"); mslc_free(error); return status; }
		FILE* output = fopen(argv[2], "wb");
		CHECK(output != NULL && fwrite(bytes, 1, size, output) == size && fclose(output) == 0);
		mslc_free(bytes);
		return 0;
	}
	CHECK(argc == 1);
	int (*legacy)(const char*, size_t, const MslcOptions*, uint8_t**, size_t*, char**, char**) = mslc_translate;
	int (*library)(const char*, size_t, const MslcOptions*, uint8_t**, size_t*, char**) = mslc_compile_library;
	CHECK(legacy != NULL && library != NULL);
	accepted("");
	accepted("// comment\n#include <metal_stdlib>\nusing namespace metal;;\n#if 0\nfloat bad;\n#endif\n");
	accepted("#define U using namespace metal;\nU\n");
	const char* declarations[] = {"float helper() { return 1; }", "float helper();", "float global;",
		"typedef float Alias;", "enum E {};", "enum {};", "struct S {};", "namespace N {}",
		"namespace N = metal;", "using Alias = float;", "using metal::float4;",
		"fragment float4 entry() { return float4(1); }"};
	for (size_t i = 0; i < sizeof(declarations) / sizeof(*declarations); ++i)
		rejected(declarations[i], NULL, NULL);
	rejected(NULL, NULL, "no source given");
	rejected("typedef Missing Alias;", NULL, NULL);
	rejected("using namespace metal", NULL, NULL);
	MslcOptions options;
	mslc_default_options(&options);
	CHECK(options.stage == MSLC_STAGE_UNKNOWN && options.localSizeX == 1 && options.localSizeY == 1
		&& options.localSizeZ == 1 && options.imageSetPolicy == MSLC_SET_COMBINED
		&& options.sourcePath == NULL && options.includeDirs == NULL && options.includeDirCount == 0);
	options.stage = MSLC_STAGE_KERNEL;
	rejected("", &options, "stage UNKNOWN");
	options.stage = (MslcStage)99;
	rejected("", &options, "stage UNKNOWN");
	mslc_default_options(&options);
	options.sourcePath = "library-origin.metal";
	rejected("#define DECL typedef float Alias;\nDECL\n", &options, "library-origin.metal:2:");
	options.includeDirCount = 1;
	rejected("", &options, "includeDirs is NULL");
	const char* dirs[] = {NULL};
	options.includeDirs = dirs;
	rejected("", &options, "NULL entry");
	mslc_default_options(&options);
	options.sourcePath = "library-api.metal";
	const char* included[] = {"float global;", "float helper() { return 1; }",
		"typedef float Alias;", "enum {};"};
	for (size_t i = 0; i < sizeof(included) / sizeof(*included); ++i) {
		FILE* header = fopen("library-api-included.h", "w");
		CHECK(header != NULL && fputs(included[i], header) >= 0 && fclose(header) == 0);
		rejected("#include \"library-api-included.h\"\n", &options, "library-api-included.h:1:");
	}
	char* error = NULL;
	CHECK(mslc_translate("", 0, NULL, NULL, NULL, NULL, &error) == 1 && error != NULL);
	mslc_free(error);
	const char* helper = "float helper() { return 1; }";
	CHECK(mslc_translate(helper, strlen(helper), NULL, NULL, NULL, NULL, &error) == 1 && error != NULL);
	mslc_free(error);
	return 0;
}
