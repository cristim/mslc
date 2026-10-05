// EXPECT: valid
// DISASM: ArrayStride 24
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 MatrixStride 8[^0-9]
//
// A half3 is 8 bytes in Metal, so a half3x3's columns are 8 bytes apart and the
// matrix is 24, not the 48 a float3x3 takes.
kernel void half_matrix_layout(device half3 *out [[buffer(0)]],
                               device const half3x3 *matrices [[buffer(1)]],
                               device const half3 *in [[buffer(2)]],
                               uint i [[thread_position_in_grid]])
{
    out[i] = matrices[i] * in[i];
}
