#include <metal_stdlib>
using namespace metal;
constant float x = 1.0;
namespace A { constant float x = 2.0; float h() { float r = 0.0; { float x = 5.0; r = x; } return r + x; } }
kernel void kern(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = A::h() + x; }
