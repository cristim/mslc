// EXPECT: valid
// DISASM: OpFunctionCall %float
// DISASM-NO-MATCH: OpFunctionParameter %v3float
#include <metal_stdlib>
using namespace metal;
template <typename T> inline __attribute__((__always_inline__)) T conv(T c) { return c * 2.0f; }
float conv(float3 c) { return c.x; }

kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = conv(1.0f); }
