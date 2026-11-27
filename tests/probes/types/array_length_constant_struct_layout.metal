// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 Offset 0
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 48
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 72
// DISASM-MATCH: OpDecorate %_arr_v3float_uint_3[_0-9]* ArrayStride 16
// DISASM-MATCH: OpDecorate %_arr_float_uint_3[_0-9]* ArrayStride 4
// DISASM-MATCH: OpDecorate %_arr__arr_float_uint_3[_0-9]*_uint_2[_0-9]* ArrayStride 12
//
// The layout of a struct whose lengths name constants is the layout of the literal spelling
// "float3 p[3]; float m[2][3]; uint n;".
constant int N = 3;
constexpr constant int R = N - 1;
struct Mixed
{
    float3 p[N];
    float m[R][N];
    uint n;
};

kernel void array_length_constant_struct_layout(device float *out [[buffer(0)]],
    constant Mixed &g [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = g.p[2].x + g.m[1][2] + float(g.n);
}
