// EXPECT: valid
// DISASM: Sqrt %float_4
// "::N::M::sqrt": an alias inside a namespace, spelled absolutely.
#include <metal_stdlib>
namespace N { namespace M = ::metal; } using namespace N; kernel void kern(device float* out [[buffer(0)]]) { out[0] = ::N::M::sqrt(4.0f); }
