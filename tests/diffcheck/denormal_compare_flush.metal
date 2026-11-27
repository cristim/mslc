#include <metal_stdlib>
using namespace metal;
// Apple flushes a denormal float to zero when it is compared or converted to bool (#377).
// The input is fixed, not seeded: element i holds the float with bit pattern i + 1, a
// denormal for every i below 8388607, so the kernel needs no buffer of denormals.
//
// Measured on the Apple GPU (MTLMathModeSafe): out[i] is 0 for all three columns, while
// the copy keeps the bits (o_bits[i] == i + 1). mslc declares no DenormFlushToZero, so
// lavapipe keeps the denormal and the compares give 1. The two backends differ on
// purpose; tests/diffcheck_denormal_test.py pins both values. Not run as a probe: a
// "// RUN:" line would expect the backends to match.
kernel void k(device uint* o_ne [[buffer(0)]], device uint* o_bool [[buffer(1)]],
	device uint* o_bits [[buffer(2)]], uint i [[thread_position_in_grid]]) {
	float f = as_type<float>(i + 1u);
	o_ne[i] = (f != 0.0f) ? 1u : 0u;
	o_bool[i] = bool(f) ? 1u : 0u;
	o_bits[i] = as_type<uint>(f);
}
