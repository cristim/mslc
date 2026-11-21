// RUN: grid=64 local=16 cases=2
// DIFFCHECK-UNUSED: ignored the kernel never reads this texture on purpose
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read> used [[texture(0)]], texture2d<float, access::read> ignored [[texture(1)]],
	device float *out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = used.read(uint2(i % 4, i / 4 % 4)).x;
}
