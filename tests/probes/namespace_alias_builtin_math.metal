// EXPECT: valid
// DISASM: Sqrt
#include <metal_stdlib>
namespace M = metal;
kernel void k(device float* out [[buffer(0)]], device float* in [[buffer(1)]]) {
    out[0] = M::sqrt(in[0]);
}
