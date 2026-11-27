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
	const char* const* macroNames;
	const char* const* macroValues;
	size_t macroCount;
	uint32_t maskGridSize;
};
#define OFFSET(field) _Static_assert(offsetof(MslcOptions, field) == offsetof(struct LegacyOptions, field), #field)
_Static_assert(sizeof(MslcOptions) == sizeof(struct LegacyOptions), "options ABI size");
OFFSET(stage); OFFSET(localSizeX); OFFSET(localSizeY); OFFSET(localSizeZ);
OFFSET(imageSetPolicy); OFFSET(sourcePath); OFFSET(includeDirs); OFFSET(includeDirCount);
OFFSET(macroNames); OFFSET(macroValues); OFFSET(macroCount); OFFSET(maskGridSize);

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
	const char* declarations[] = {"float global;",
		"typedef float Alias;", "enum E {};", "enum {};", "struct S {};", "namespace N {}",
		"namespace N = metal;", "using Alias = float;", "using metal::float4;"};
	for (size_t i = 0; i < sizeof(declarations) / sizeof(*declarations); ++i)
		rejected(declarations[i], NULL, NULL);
	/* A unit with functions is a Linkage object with content, not the empty one. */
	const char* objects[] = {"float helper() { return 1; }", "float helper();",
		"fragment float4 entry() { return float4(1); }"};
	for (size_t i = 0; i < sizeof(objects) / sizeof(*objects); ++i) {
		uint8_t* bytes = NULL;
		size_t size = 0;
		char* error = NULL;
		CHECK(mslc_compile_library(objects[i], strlen(objects[i]), NULL, &bytes, &size, &error) == 0);
		CHECK(bytes != NULL && size >= 56 && error == NULL);
		mslc_free(bytes);
	}
	/* NUL characters are ignored, as clang does: after the last token (a length that counts the
	 * terminator), between two tokens and inside a comment. */
	{
		static const char withNul[] = "float helper() { return\0 1; }\n// a\0b\n\0";
		uint8_t* bytes = NULL;
		size_t size = 0;
		char* error = NULL;
		CHECK(mslc_compile_library(withNul, sizeof(withNul), NULL, &bytes, &size, &error) == 0);
		CHECK(bytes != NULL && size >= 56 && error == NULL);
		mslc_free(bytes);
	}
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
	const char* included[] = {"float global;",
		"typedef float Alias;", "enum {};"};
	for (size_t i = 0; i < sizeof(included) / sizeof(*included); ++i) {
		FILE* header = fopen("library-api-included.h", "w");
		CHECK(header != NULL && fputs(included[i], header) >= 0 && fclose(header) == 0);
		rejected("#include \"library-api-included.h\"\n", &options, "library-api-included.h:1:");
	}
	/* A helper in an included header is content too: accepted, where the other
	 * declarations above are still refused with the header's own position. */
	{
		FILE* header = fopen("library-api-included.h", "w");
		CHECK(header != NULL && fputs("float helper() { return 1; }", header) >= 0 && fclose(header) == 0);
		const char* include = "#include \"library-api-included.h\"\n";
		uint8_t* bytes = NULL;
		size_t size = 0;
		char* diagnostic = NULL;
		CHECK(mslc_compile_library(include, strlen(include), &options, &bytes, &size, &diagnostic) == 0);
		CHECK(bytes != NULL && size > 56 && diagnostic == NULL);
		mslc_free(bytes);
  }
	{
		// A helper emitted once for two kernels: both list the function constant it reads.
		const char* shared =
			"constant int c [[function_constant(0)]];\n"
			"template <typename T> T leaf() { return T(c); }\n"
			"int shared() { return leaf<int>(); }\n"
			"kernel void a(device int* out [[buffer(0)]]) { out[0] = shared(); }\n"
			"kernel void b(device int* out [[buffer(0)]]) { out[0] = shared() + 1; }\n";
		char* reflection = NULL;
		char* error = NULL;
		CHECK(mslc_translate(shared, strlen(shared), NULL, NULL, NULL, &reflection, &error) == 0 && reflection != NULL);
		int listed = 0;
		for (const char* at = reflection; (at = strstr(at, "{ \"name\": \"c\", \"type\": \"int\"")) != NULL; at++) {
			listed++;
		}
		CHECK(listed == 2);
		mslc_free(reflection);
	}
  // No entry point: a library with no functions, which has no module, and a reflection with no entry point.
	const char* noEntryPoints[] = { "", "float helper() { return 1; }" };
	for (size_t i = 0; i < sizeof(noEntryPoints) / sizeof(noEntryPoints[0]); i++) {
		uint8_t* spirv = (uint8_t*)1;
		size_t spirvSize = 99;
		char* reflection = NULL;
		char* error = NULL;
		CHECK(mslc_translate(noEntryPoints[i], strlen(noEntryPoints[i]), NULL, &spirv, &spirvSize, &reflection, &error) == 0);
		CHECK(spirv == NULL && spirvSize == 0 && error == NULL);
		CHECK(reflection != NULL && strstr(reflection, "\"entry_points\": [\n\t]") != NULL);
		mslc_free(reflection);
	}
  // A stage asked for is an entry point the source has to have.
	MslcOptions kernel;
	mslc_default_options(&kernel);
	kernel.stage = MSLC_STAGE_KERNEL;
	char* error = NULL;
	CHECK(mslc_translate("", 0, &kernel, NULL, NULL, NULL, &error) == 1 && error != NULL);
	mslc_free(error);
	return 0;
}
