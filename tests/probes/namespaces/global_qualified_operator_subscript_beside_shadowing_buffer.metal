// EXPECT: valid
// RUN: grid=4 local=4 cases=3
#include <metal_stdlib>
using namespace metal;
struct S {
	float v;
	float operator[](int i) const constant { return v + float(i) * 3.0f; }
};
constant S s = { .v = 5.0f };
kernel void kern(device float* s [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
	s[gid] = ::s[int(gid & 3u)] + float(gid);
}
