// EXPECT: error reference to "x" is ambiguous
// Apple: B nominates C, whose names appear at global scope next to ::x; A has no x.
#include <metal_stdlib>
using namespace metal;
constant float x = 1.0;
namespace C { constant float x = 3.0; }
namespace A { namespace B { using namespace C; float g() { return x; } } }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = A::B::g(); }
