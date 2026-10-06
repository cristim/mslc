// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 8
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 12
//
// A bool member is one byte. The member after it starts at its own alignment,
// and the struct is 12 bytes: this used to give every member the offset of the
// one before, because the bool's size came out as zero.
struct Item
{
    float weight;
    bool live;
    int count;
};

kernel void bool_struct_member_offsets(device uint *out [[buffer(0)]], device const Item *items [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (items[i].live) {
        out[i] = uint(items[i].count);
    }
}
