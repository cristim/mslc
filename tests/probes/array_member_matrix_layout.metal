// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 MatrixStride 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 ColMajor
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 192
// DISASM-MATCH: OpDecorate %_arr_mat4v4float_uint_3[_0-9]* ArrayStride 64
//
// An array of matrices: each matrix is 64 bytes, so the array steps by 64 and the
// member carries MatrixStride 16 and ColMajor, which SPIR-V takes on an array of
// matrices as it does on a matrix. The count after it starts at 192.
struct Skin
{
    float4x4 boneMatrices[3];
    uint count;
};

kernel void array_member_matrix_layout(device float *out [[buffer(0)]],
    constant Skin &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = s.boneMatrices[s.count][1][2];
}
