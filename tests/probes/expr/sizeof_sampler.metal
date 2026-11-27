// EXPECT: valid
// DISASM-MATCH: OpConstant %ulong 8[^0-9]
// DISASM-MATCH: OpConstant %ulong 24[^0-9]
// DISASM-NO-MATCH: OpConstant %ulong 4[^0-9]
//
// A sampler is a handle of 8 bytes, and an array of three is 24 (Apple: 8 and 24).
#include <metal_stdlib>
using namespace metal;

kernel void sizeof_sampler(device ulong *out [[buffer(0)]])
{
    out[0] = sizeof(sampler);
    out[1] = sizeof(sampler[3]);
}
