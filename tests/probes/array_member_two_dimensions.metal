// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpDecorate %_arr_float_uint_3[_0-9]* ArrayStride 4
// DISASM-MATCH: OpDecorate %_arr__arr_float_uint_3[_0-9]*_uint_2[_0-9]* ArrayStride 12
//
// float m[2][3]: the inner array steps by 4 and the outer by 12.
struct Grid
{
    uint n;
    float m[2][3];
};

kernel void array_member_two_dimensions(device float *out [[buffer(0)]],
    constant Grid &g [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = g.m[1][2] + g.m[i & 1][0];
}
