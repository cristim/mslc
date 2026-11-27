// EXPECT: error copy construction of "S" with parentheses when it declares a constructor template is not lowered yet
// Refs #341: a 1-arg constructor template must not hijack copy construction S s2(s1).
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	template <typename U> S(U x) : a(1.0f) {}
};
kernel void k(device float* out [[buffer(0)]])
{
	S s1(2);
	S s2(s1);
	out[0] = s2.a;
}
