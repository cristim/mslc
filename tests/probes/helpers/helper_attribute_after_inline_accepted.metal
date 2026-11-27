// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: METAL_FUNC is "inline __attribute__((__always_inline__))", an attribute after the
// specifier and ahead of the return type, as MetalPetal's helpers spell it; also noinline.
#include <metal_stdlib>
using namespace metal;
METAL_FUNC float twice(float x) { return x * 2.0f; }
__attribute__((noinline)) inline float thrice(float x) { return x * 3.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = twice(1.0f) + thrice(1.0f); }
