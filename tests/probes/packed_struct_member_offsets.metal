// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 0 Offset 0
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 4
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 16
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 20
//
// A packed_float3 is 12 bytes aligned to 4, so it starts right after the float
// before it and the float after it starts at 16. As a float3 these would be 16,
// 32 and a stride of 48.
struct Record
{
    float tag;
    packed_float3 position;
    float weight;
};

kernel void packed_struct_member_offsets(device Record *out [[buffer(0)]],
                                         device const Record *in [[buffer(1)]],
                                         uint index [[thread_position_in_grid]])
{
    out[index].position = in[index].position;
    out[index].weight = in[index].weight;
}
