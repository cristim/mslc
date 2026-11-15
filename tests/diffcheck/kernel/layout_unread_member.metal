// RUN: grid=64 local=16 cases=2
// A constant struct whose middle members (two floats and a matrix) the kernel never reads. A wrong Offset, a swap of two offsets or a row-major matrix is
// invisible in the outputs (nothing reads those bytes) but breaks every app that packs the
// struct the way Metal lays it out, so diffcheck compares the offsets in mslc's SPIR-V with
// Apple's reflection instead.
#include <metal_stdlib>
using namespace metal;
struct P {
	float a;
	float unreadX;
	float unreadY;
	float4x4 unreadM;
	float b;
};
kernel void k(constant P &p [[buffer(0)]], device float *out [[buffer(1)]], device float *outB [[buffer(2)]],
	uint i [[thread_position_in_grid]]) {
	out[i] = p.a * float(i);
	outB[i] = p.b;
}
