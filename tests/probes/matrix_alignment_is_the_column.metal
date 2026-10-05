// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 8[^0-9]
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 MatrixStride 8[^0-9]
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 32[^0-9]
//
// A matrix aligns to its column, not to its scalar: a float3x2 is three
// 8-byte float2 columns, so after a float it starts at 8 rather than 4, and the
// float after it starts at 8 + 24 = 32.
struct Mixed
{
    float a;
    float3x2 m;
    float b;
};

kernel void matrix_alignment_is_the_column(device float2 *out [[buffer(0)]],
                                           constant Mixed &mixed [[buffer(1)]],
                                           device const float3 *in [[buffer(2)]],
                                           uint i [[thread_position_in_grid]])
{
    out[i] = mixed.m * in[i];
}
