// EXPECT: error parameter "p" of helper function "f" is a pointer outside the thread, device and constant address spaces
#include <metal_stdlib>
using namespace metal;
float f(float* p) { return p[0]; }
kernel void helper_pointer_no_space_rejected(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(out); }
