// EXPECT: valid
// DISASM: OpConvertFToS %int
// DISASM: OpConvertFToU %uint
// Refs #391: C-style integer casts whose operand is parenthesised keep parsing as casts.
#include <metal_stdlib>
using namespace metal;

kernel void cast_c_style_int_parenthesised_operand(device int* out [[buffer(0)]],
    device const float* in [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    float x = in[i];
    int a = (int)(x + 1.0f);
    uint b = (unsigned int)(x + 2.0f);
    int c = (const int)(x + 3.0f);
    out[i] = a + int(b) + c;
}
