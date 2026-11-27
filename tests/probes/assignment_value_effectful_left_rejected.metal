// EXPECT: error an assignment whose left operand has a call, an increment or an assignment in it cannot be used as a value yet
// The value is the left operand read again, which an increment in it would change.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { uint j = 0; float x = (out[j++] = 2.0f); out[i] = x; }
