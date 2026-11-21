// EXPECT: valid
// DISASM: Sqrt %float_4
// An alias declared in N is visible unqualified under "using namespace N;" and is the library's.
#include <metal_stdlib>
namespace N { namespace M = ::metal; } using namespace N; kernel void kern(device float* out [[buffer(0)]]) { out[0] = M::sqrt(4.0f); }
