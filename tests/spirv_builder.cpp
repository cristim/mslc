// Builder::finalize on a builder nothing was emitted into: its header promises
// false, because such a module has no entry point and is not loadable.

#include "spirv.h"

#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
	mslc::spirv::Builder builder;
	std::vector<uint8_t> bytes;
	if (builder.hasEntryPoint()) {
		std::fprintf(stderr, "a blank builder reports an entry point\n");
		return 1;
	}
	if (builder.finalize(bytes)) {
		std::fprintf(stderr, "finalize returned true for a blank builder\n");
		return 1;
	}
	if (!bytes.empty()) {
		std::fprintf(stderr, "finalize wrote %zu bytes for a blank builder\n", bytes.size());
		return 1;
	}
	return 0;
}
