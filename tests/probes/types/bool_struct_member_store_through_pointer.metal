// EXPECT: valid
// DISASM-MATCH: OpSelect %uchar
// DISASM-MATCH: OpStore %[0-9]+ %[0-9]+ Aligned 1
//
// A bool member written through a pointer to its struct is converted to a byte
// at the member's own address.
struct Item
{
    float weight;
    bool live;
};

kernel void bool_struct_member_store_through_pointer(device Item *items [[buffer(0)]], device const float *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    items[i].live = in[i] > 0.5;
    items[i].weight = in[i];
}
