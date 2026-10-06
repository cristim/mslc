// EXPECT: error reference to "x" is ambiguous
// Apple: a using directive in A puts C::x at global scope, where ::x is also found.
#include <metal_stdlib>
using namespace metal;
constant float x = 1.0;
namespace C { constant float x = 3.0; }
namespace A { using namespace C; float g() { return x; } }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = A::g(); }
