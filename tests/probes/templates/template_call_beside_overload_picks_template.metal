// EXPECT: valid
// DISASM-NO-MATCH: OpFunctionParameter %v3float
// DISASM: OpFMul %float
// Refs #341: a helper of the template's name must not take the call. Without the
// check, a float argument would be broadcast into the float3 helper, which is a
// miscompile (the template, instantiated for float, is what Apple calls).
#include <metal_stdlib>
using namespace metal;
template <typename T> T conv(T c) { return c * 2.0f; }
float3 conv(float3 c) { return c; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = conv(1.0f); }
