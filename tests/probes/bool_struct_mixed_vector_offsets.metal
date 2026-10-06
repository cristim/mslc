// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 2
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 3 Offset 8
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 4 Offset 12
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 5 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 6 Offset 20
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 24
//
// Offsets measured on an Apple GPU: a bool2 sits at a multiple of 2, a bool3
// and a bool4 at a multiple of 4, the struct is 24 bytes.
struct Mixed
{
    bool a;
    bool2 b;
    bool c;
    bool3 d;
    bool e;
    bool4 f;
    bool g;
};

kernel void bool_struct_mixed_vector_offsets(device uint *out [[buffer(0)]], device const Mixed *items [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    Mixed m = items[i];
    out[i] = uint(m.a) + 2u * uint(m.b.y) + 4u * uint(m.c) + 8u * uint(m.d.z)
        + 16u * uint(m.e) + 32u * uint(m.f.w) + 64u * uint(m.g);
}
