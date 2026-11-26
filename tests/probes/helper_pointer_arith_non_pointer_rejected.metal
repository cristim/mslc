// EXPECT: error argument 1 of the call to "f" is arithmetic on a pointer mslc does not lower; a pointer plus or minus an integer is the form that is lowered
#include <metal_stdlib>
using namespace metal;
float f(device float* p, uint i) { return p[i]; }
kernel void helper_pointer_arith_non_pointer_rejected(device float* out [[buffer(0)]],
                                                      device float* in [[buffer(1)]],
                                                      uint i [[thread_position_in_grid]])
{ out[i] = f(i + 1, 0); }
