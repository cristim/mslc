// EXPECT: valid
// DISASM-NO-MATCH: OpBitcast %_struct
// DISASM-MATCH: OpINotEqual %bool
// DISASM-MATCH: OpSelect %uchar
//
// A struct with a bool member copied between a buffer and a local goes member
// by member: the buffer's member is a byte and the local's is a bool.
struct Item
{
    float weight;
    bool live;
};

kernel void bool_struct_copy_buffer_local(device Item *out [[buffer(0)]], device const Item *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    Item z = in[i];
    z.live = !z.live;
    out[i] = z;
}
