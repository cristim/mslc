// EXPECT: valid
// DISASM: Sqrt %float_4
// "::M::sqrt": an alias spelled with a leading :: still names the library.
#include <metal_stdlib>
namespace M = ::metal; kernel void kern(device float* out [[buffer(0)]]) { out[0] = ::M::sqrt(4.0f); }
