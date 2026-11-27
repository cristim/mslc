// EXPECT: valid
// RUN: grid=4 local=4 cases=3
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 3 Offset 32
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 56
// DISASM-MATCH: OpDecorate %_arr__struct_[0-9]+_uint_2[_0-9]* ArrayStride 48
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 8
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 3 Offset 16
//
// Structs nested two deep, whose innermost one holds a bool, a packed vector, a matrix and an array:
// Inner is 8-aligned (the float2x2) and 48 bytes, so Middle puts it at 8 and its pair at 56 and is 152
// bytes; the empty struct takes one byte at 4 in Outer, which puts Middle at 16 and makes Outer 168.
// Apple's compiler gives the same offsets and sizes (offsetof and sizeof read back through a template
// argument). Every member is read through the buffer and through a copied local.
#include <metal_stdlib>

struct Empty
{
};

struct Inner
{
    bool flag;
    packed_uint3 packed;
    float2x2 m;
    uint list[3];
};

struct Middle
{
    uint tag;
    Inner one;
    Inner pair[2];
};

struct Outer
{
    uint first;
    Empty nothing;
    uint second;
    Middle middle;
};

kernel void struct_member_nested_mixed_layout(device float *out [[buffer(0)]],
    constant Outer &p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    const uint k = i & 1;
    const Middle copy = p.middle;
    out[i] = float(p.first + p.second + p.middle.tag + p.middle.one.packed[i % 3] + p.middle.pair[k].list[i % 3]
        + (p.middle.pair[1 - k].flag ? 1u : 0u) + copy.pair[k].packed.z + copy.one.list[2])
        + p.middle.one.m[k][1] + copy.pair[1 - k].m[1][k]
        + float(sizeof(Inner) + sizeof(Middle) + sizeof(Outer) + sizeof(Empty));
}
