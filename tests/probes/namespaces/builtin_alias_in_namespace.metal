// EXPECT: valid
// DISASM: Sqrt %float_4
// An alias declared in a namespace is reached as N::M and is the library's metal.
#include <metal_stdlib>
namespace N { namespace M = ::metal; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::M::sqrt(4.0f); }
