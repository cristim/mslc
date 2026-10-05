// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4[^0-9]
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 MatrixStride 4[^0-9]
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 16[^0-9]
//
// A half3x2 is three 4-byte half2 columns, so after a half it starts at 4
// rather than 2, and the half after it starts at 4 + 12 = 16.
struct Mixed
{
    half a;
    half3x2 m;
    half b;
};

kernel void half_matrix_alignment_is_the_column(device half2 *out [[buffer(0)]],
                                                constant Mixed &mixed [[buffer(1)]],
                                                device const half3 *in [[buffer(2)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = mixed.m * in[i];
}
