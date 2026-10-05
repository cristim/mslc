// EXPECT: error struct "Item" has the bool member "live" and is used as a buffer's layout
//
// Every member of such a struct used to be given offset 0, because the bool's
// size came out as zero bytes.
struct Item
{
    float weight;
    bool live;
};

kernel void bool_struct_member_in_buffer_rejected(device uint *out [[buffer(0)]], device const Item *items [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    if (items[i].live) {
        out[i] = 1u;
    }
}
