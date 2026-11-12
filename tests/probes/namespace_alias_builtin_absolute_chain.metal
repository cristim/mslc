// EXPECT: valid
// DISASM: Sqrt
#include <metal_stdlib>
namespace N { namespace M = ::metal; }
namespace X = N::M;
kernel void k(device float* out [[buffer(0)]], device float* input [[buffer(1)]]) {
    out[0] = ::X::sqrt(input[0]);
}
