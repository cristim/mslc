// EXPECT: valid
// RUN: grid=4 local=4 cases=3
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 48
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 3 Offset 64
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 4 Offset 128
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 5 Offset 192
// DISASM-MATCH: OpDecorate %_arr__struct_[0-9]+_uint_2[_0-9]* ArrayStride 32
// REFLECT: "kind": "Buffer"
//
// A struct member of a buffer struct, by value and as both spellings of an array, is laid out whole:
// at the next multiple of its own alignment (16, from the uint3), and an array of it steps by its
// size rounded to that alignment (32). Apple's compiler gives the same offsets (offsetof, sizeof and
// alignof checked in a static_assert). Wrong offsets validate, so the numbers are pinned.
#include <metal_stdlib>

struct Light
{
    uint tag;
    uint3 direction;
};

struct Lights
{
    uint first;
    Light one;
    uint middle;
    Light pair[2];
    metal::array<Light, 2> both;
    uint last;
};

kernel void struct_member_nested_buffer_layout(device uint *out [[buffer(0)]],
    constant Lights &p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    const uint k = i & 1;
    out[i] = p.first + p.one.direction[i % 3] + p.middle + p.pair[k].tag + p.pair[k].direction.z
        + p.both[1 - k].direction.y + p.both[k].tag + p.last + uint(sizeof(Lights)) + uint(sizeof(Light));
}
