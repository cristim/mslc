// RUN: grid=64 local=16 cases=2
// DIFFCHECK-UNUSED: used this texture is read, so the declaration is stale
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read> used [[texture(0)]], device float *out [[buffer(0)]],
	uint i [[thread_position_in_grid]]) {
	out[i] = used.read(uint2(i % 4, i / 4 % 4)).x;
}
