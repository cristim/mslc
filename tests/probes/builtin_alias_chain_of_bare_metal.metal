// EXPECT: valid
// DISASM: Sqrt %float_4
// An alias of an alias of metal (spelled without the leading ::) is metal too.
#include <metal_stdlib>
namespace N = metal;
namespace M = N;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = M::sqrt(4.0f); }
