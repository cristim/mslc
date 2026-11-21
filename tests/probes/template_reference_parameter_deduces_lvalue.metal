// EXPECT: valid
// DISASM: OpFunctionCall %float
// DISASM: OpFunctionParameter %v3float
// Refs #341: chameleonrt's luminance(thread const V &c): V is the lvalue's type (float3), and the
// const reference is passed by value.
#include <metal_stdlib>
using namespace metal;
template <typename V> float luminance(thread const V &c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }
kernel void k(device float* out [[buffer(0)]]) { float3 base = float3(1.0f, 2.0f, 3.0f); out[0] = luminance(base); }
