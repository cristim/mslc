// EXPECT: error argument 1 of the call to "f"
#include <metal_stdlib>
using namespace metal;
struct A { float x; };
struct B { float x; };
float f(A a) { return a.x; }
kernel void k(device float* out [[buffer(0)]], device const B* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{ out[i] = f(in[i]); }
