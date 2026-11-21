// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %float %[0-9]+ %float_4
// DISASM: Sqrt %float_4_0
// M is an alias of the user's U, whose sqrt is a helper (a call); L is the library's (an
// OpExtInst): the alias never changes which namespace it names.
#include <metal_stdlib>
namespace U { float sqrt(float a) { return 7.0; } }
namespace M = U;
namespace L = ::metal;
kernel void kern(device float* out [[buffer(0)]]) { out[0] = M::sqrt(4.0f) + L::sqrt(4.0f); }
