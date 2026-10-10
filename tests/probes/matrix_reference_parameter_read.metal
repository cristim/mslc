// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %[A-Za-z0-9_]+ 0 MatrixStride 16
// DISASM-MATCH: OpMemberDecorate %[A-Za-z0-9_]+ 0 ColMajor
// A matrix reached by reference is the one member of a wrapper struct, which is where
// SPIR-V allows its MatrixStride and ColMajor; the reference is the address of that member.
kernel void matrix_reference_parameter_read(device float4 *out [[buffer(0)]],
                                            constant float4x4 &m [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = m * out[i];
}
