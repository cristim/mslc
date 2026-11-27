// EXPECT: valid
// RUN: grid=4 local=4 cases=3
#include <metal_stdlib>
using namespace metal;
struct T {
	float w;
	float fn() const { return w; }
};
struct S {
	float v;
	float fn() const constant { return v + 2.0f; }
};
constant S s = { .v = 11.0f };
kernel void kern(device float* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
	T s = { .w = float(gid) + 1.0f };
	out[gid] = ::s.fn() + s.fn();
}
