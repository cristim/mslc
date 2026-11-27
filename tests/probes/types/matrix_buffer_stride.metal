// EXPECT: valid
// DISASM: ArrayStride 48
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 MatrixStride 16[^0-9]
//
// An array of float3x3 steps by the matrix's own size, three 16-byte columns,
// and the runtime array's member carries the column stride, which SPIR-V only
// allows on a struct member.
kernel void matrix_buffer_stride(device float3 *out [[buffer(0)]],
                                 device const float3x3 *matrices [[buffer(1)]],
                                 device const float3 *in [[buffer(2)]],
                                 uint i [[thread_position_in_grid]])
{
    out[i] = matrices[i] * in[i];
}
