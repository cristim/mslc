// EXPECT: valid
// DISASM: OpTypeArray %float %uint_5
// DISASM: OpTypeArray %float %uint_6
// DISASM: OpTypeArray %float %uint_32
// DISASM: OpTypeArray %float %uint_20
// REFLECT: "metal_index": 9
//
// sizeof folds wherever a constant integer is read: an array length, a named constant it
// initialises, and an attribute index through a cast to an int type (Apple rejects a bare
// ulong there). Apple's compiler reads the same lengths: 16 / 4 + 1, 3 - 1 + 4, a ulong
// constexpr of 4 * 8, and a struct of 20 bytes.
#include <metal_stdlib>
using namespace metal;
struct Five { float a[5]; };
constant constexpr ulong kWide = sizeof(float2) * 4;
constant int kPad = sizeof(Five);
kernel void sizeof_in_constant_expressions(device float *out [[buffer(0)]],
    device float *extra [[buffer((uint)sizeof(ulong) + 1)]])
{
    float a[sizeof(float3) / 4 + 1];
    float b[3 - 1 + sizeof(float)];
    float c[kWide];
    float d[kPad];
    a[0] = extra[0];
    b[0] = extra[1];
    c[0] = extra[2];
    d[0] = extra[3];
    out[0] = a[0] + b[0] + c[0] + d[0];
}
