// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[0-9]+ Fma %
#include <metal_stdlib>
using metal::precise::fma;
kernel void math_using_precise_at_file_scope(
    device const float* in [[buffer(0)]],
    device float* out [[buffer(1)]])
{
    out[0] = fma(in[0], in[1], in[2]);
}
