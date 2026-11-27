// EXPECT: valid
// DISASM: OpFunctionCall %float
// Two helpers of one name that take a different number of parameters: each call is the
// overload of its arity. Apple accepts this; mslc took one signature per name.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x * 2.0f; }
float f(float x, float y) { return x + y; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(3.0f) + f(3.0f, 4.0f); }
