// EXPECT: error a conditional or comma expression as an argument chooses among the overloads
// Apple calls g(thread S &) here (a and b are lvalues); mslc would have called g(const thread S &).
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; float g(thread S &s) { s.v = 9.0f; return 1.0f; } float g(const thread S &) { return 2.0f; } }
kernel void k(device float *out [[buffer(0)]]) {
	bool c = out[5] > 0.0f; A::S a; A::S b; a.v = 1.0f; b.v = 2.0f;
	out[0] = g(c ? a : b);
}
