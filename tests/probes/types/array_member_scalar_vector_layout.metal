// EXPECT: valid
// RUN: grid=5 local=5 cases=3
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 32
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 3 Offset 64
// DISASM-MATCH: OpDecorate %_arr_float_uint_5[_0-9]* ArrayStride 4
// DISASM-MATCH: OpDecorate %_arr_v3float_uint_2[_0-9]* ArrayStride 16
// REFLECT: "kind": "Buffer"
//
// An array member steps by its element's size in Metal's layout: float[5] by 4
// and float3[2] by 16 (a float3 is 16 bytes). The float3 array starts at 32, the
// next multiple of 16 after the 24 bytes the first two members take, and the
// tail after it starts at 64. Wrong offsets validate, so the numbers are pinned.
struct Params
{
    float scale;
    float lights[5];
    float3 positions[2];
    float tail;
};

kernel void array_member_scalar_vector_layout(device float *out [[buffer(0)]],
    constant Params &p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = p.scale + p.lights[i] + p.positions[1].y + p.tail;
}
