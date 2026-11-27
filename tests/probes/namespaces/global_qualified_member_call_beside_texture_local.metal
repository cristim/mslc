// EXPECT: valid
// RUN: grid=4 local=4 cases=3
#include <metal_stdlib>
using namespace metal;
struct S {
	float v;
	float fn() const constant { return v; }
};
constant S t = { .v = 7.0f };
kernel void kern(texture2d<float> t [[texture(0)]], device float* out [[buffer(0)]],
	uint gid [[thread_position_in_grid]]) {
	out[gid] = ::t.fn() + t.read(uint2(gid & 3u, 0u)).x;
}
