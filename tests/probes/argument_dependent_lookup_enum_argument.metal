// EXPECT: valid
// The namespace of an enum argument is searched too: f(A::x) is A::f(E) (Apple), not f(int).
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 2.0f; }
namespace A { enum E { x }; float f(E e) { return 1.0f; } }
kernel void k(device float* out [[buffer(0)]])
{ out[0] = f(A::x); }
