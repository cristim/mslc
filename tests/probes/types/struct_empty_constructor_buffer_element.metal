// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 16
// DISASM-MATCH: OpDecorate %_runtimearr__struct_[0-9]+ ArrayStride 32
//
// An empty constructor adds no member and no padding: Apple lays this struct out
// as the plain one, a float2 at 0, a float4 at 16, 32 bytes in all.
struct Item
{
    Item() {}
    float2 a;
    float4 b;
};

kernel void k(device float *out [[buffer(0)]], device const Item *items [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = items[i].a.x + items[i].b.w;
}
