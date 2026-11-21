// EXPECT: valid
// DISASM-MATCH: OpDecorate %_arr_float_uint_3[_0-9]* ArrayStride 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 12
//
// A struct member's length names a constant declared above the struct.
constant uint N = 3;
struct Grid
{
    float a[N];
    uint n;
};

kernel void array_length_named_constant_struct_member(device float *out [[buffer(0)]],
    constant Grid &g [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = g.a[2] + float(g.n);
}
