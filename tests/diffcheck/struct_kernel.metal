#include <metal_stdlib>
using namespace metal;
// RUN: grid=32 local=4 seed=3 cases=3
struct S {
	float3 a;
	uint n;
	bool flag;
	half h;
	float2 p[2];
};
kernel void k(device const S* in [[buffer(0)]], device S* out [[buffer(1)]],
	constant uint& scale [[buffer(2)]], uint i [[thread_position_in_grid]]) {
	S s = in[i];
	s.a = s.a * 2.0f;
	s.n = s.n * scale + 1u;
	s.flag = !s.flag;
	s.h = s.h + half(1.0);
	s.p[1] = s.p[0] + s.p[1];
	out[i] = s;
}
