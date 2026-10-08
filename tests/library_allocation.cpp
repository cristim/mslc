#include "mslc/mslc.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void mslc_test_library_fail_allocation(int site);
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); std::exit(1); } } while (0)

int main() {
	for (int omitted = 0; omitted < 4; ++omitted) {
		uint8_t* bytes = reinterpret_cast<uint8_t*>(1);
		size_t size = 99;
		char* error = reinterpret_cast<char*>(1);
		mslc_test_library_fail_allocation(1);
		CHECK(mslc_compile_library("", 0, nullptr, omitted & 1 ? nullptr : &bytes,
			omitted & 2 ? nullptr : &size, &error) == 2);
		CHECK((omitted & 1 || bytes == nullptr) && (omitted & 2 || size == 0) && error != nullptr);
		mslc_free(error);
		CHECK(mslc_compile_library("", 0, nullptr, omitted & 1 ? nullptr : &bytes,
			omitted & 2 ? nullptr : &size, nullptr) == 2);
	}
	const char* sources[] = {nullptr, "typedef float Alias;"};
	for (const char* source : sources) {
		for (int omitBytes = 0; omitBytes < 2; ++omitBytes) {
			uint8_t* bytes = reinterpret_cast<uint8_t*>(1);
			size_t size = 99;
			char* error = reinterpret_cast<char*>(1);
			mslc_test_library_fail_allocation(2);
			CHECK(mslc_compile_library(source, source ? std::strlen(source) : 0, nullptr,
				omitBytes ? nullptr : &bytes, &size, &error) == 2);
			CHECK((omitBytes || bytes == nullptr) && size == 0 && error == nullptr);
			CHECK(mslc_compile_library(source, source ? std::strlen(source) : 0, nullptr,
				omitBytes ? nullptr : &bytes, &size, nullptr) == 1);
		}
	}
	mslc_test_library_fail_allocation(0);
	uint8_t* bytes = nullptr;
	size_t size = 0;
	CHECK(mslc_compile_library("", 0, nullptr, &bytes, &size, nullptr) == 0 && size == 56);
	mslc_free(bytes);
	return 0;
}
