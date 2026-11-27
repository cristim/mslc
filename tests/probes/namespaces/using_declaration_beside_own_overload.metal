// EXPECT: valid
// A using declaration beside the namespace's own f: f(int) is A::f through it, f(1.0f) is B::f.
#include <metal_stdlib>
using namespace metal;
namespace A { float f(int x) { return 1.0f; } }
namespace B {
	using A::f;
	float f(float x) { return 2.0f; }
	float g(int i) { return f(i) + f(1.0f); }
}
kernel void k(device float* out [[buffer(0)]])
{ out[0] = B::g(1); }
