// EXPECT: valid
// DISASM: Sqrt %float_4
// An alias declared in a function body, aliasing the library's metal.
#include <metal_stdlib>
kernel void kern(device float* out [[buffer(0)]]) { namespace M = metal; out[0] = M::sqrt(4.0f); }
