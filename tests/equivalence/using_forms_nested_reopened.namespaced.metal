#include <metal_stdlib>
using namespace metal;
namespace A { constant float a = 1.0; namespace B { constant float b = 2.0; float f(float x) { return x + a + b; } } }
namespace A { float g(float x) { return B::f(x) * a; } }
namespace C { using namespace A; float h(float x) { return g(x) + B::b; } }
using A::g;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = g(float(i)) + C::h(float(i)) + A::B::f(1.0); }
