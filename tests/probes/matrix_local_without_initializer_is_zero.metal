// EXPECT: valid
// DISASM-MATCH: %[0-9]+ = OpConstantNull %mat4v4float
// DISASM-NOT: OpConstant %mat4v4float
// A matrix local declared without a value is stored OpConstantNull, the zero of
// a composite, rather than an OpConstant with no words, which is what a scalar
// zero would be for it.
kernel void matrix_local_without_initializer_is_zero(device float4 *out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    float4x4 m;
    out[i] = m[0];
}
