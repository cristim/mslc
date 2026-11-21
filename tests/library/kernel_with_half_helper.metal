// A kernel that never mentions half, plus an exported helper that does: the
// helper is emitted after the entry points, and the module still needs the
// 16-bit SignedZeroInfNanPreserve mode on the entry point (#368).
#include <metal_stdlib>
using namespace metal;

float rounded_to_half(float value) {
	return float(half(value));
}

kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = 1.0;
}
