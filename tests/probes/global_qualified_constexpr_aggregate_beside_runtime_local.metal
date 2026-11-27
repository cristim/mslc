// EXPECT: valid
// RUN: grid=4 local=4 cases=3
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
struct Outer { S inner; float x; };
constant constexpr S cs = { .a = 3.0f, .b = 4.0f };
constant int ck = 5;
kernel void kern(device float* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
	S cs = { .a = 10.0f, .b = 20.0f };
	constexpr Outer nested = { .inner = ::cs, .x = 5.0f };
	constexpr int ck = { ::ck };
	constexpr int2 ck2 = { ck, 1 };
	out[gid] = nested.inner.a + nested.x + cs.b + float(ck2.x) + float(gid);
}
