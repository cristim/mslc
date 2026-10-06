#include <metal_stdlib>
using namespace metal;
namespace C { constant float x = 3.0; }
namespace A { constant float x = 2.0; using namespace C; float g() { return x; } }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = A::g(); }
