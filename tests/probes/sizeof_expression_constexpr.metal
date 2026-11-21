// EXPECT: valid
// DISASM: OpConstant %ulong 24
//
// A constexpr initialised from sizeof of a declared array is a constant: Apple reads 24.
#include <metal_stdlib>
using namespace metal;
kernel void sizeof_expression_constexpr(device ulong *out [[buffer(0)]])
{
    float table[6];
    constexpr ulong n = sizeof(table);
    out[0] = n;
}
