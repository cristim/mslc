// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: the call deduces T from its argument and instantiates the template.
#include <metal_stdlib>
using namespace metal;
template <typename T> T f(T x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = f(1.0f); }
