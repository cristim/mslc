// EXPECT: valid
// DISASM: OpFunctionCall
#include <metal_stdlib>
using namespace metal;
float inner(float x) { return x + 1.0f; }
float outer(float x) { return inner(x) * inner(x + 1.0f); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = outer(1.0f); }
