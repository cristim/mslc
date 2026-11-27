// EXPECT: valid
// The namespace of a struct argument is searched too: f(s) is A::f(S) beside the file's
// f(float) (Apple).
#include <metal_stdlib>
using namespace metal;
float f(float x) { return 1.0f; }
namespace A {
	struct S { float v; };
	float f(S s) { return s.v + 10.0f; }
}
kernel void k(device float* out [[buffer(0)]])
{
	A::S s;
	s.v = 1.0f;
	out[0] = f(s) + f(2.0f);
}
