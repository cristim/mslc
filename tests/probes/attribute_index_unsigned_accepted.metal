// EXPECT: valid
// REFLECT: "metal_index": 5

//
// An index written with a u is a uint, which Apple takes.
kernel void attribute_index_unsigned_accepted(device uint* out [[buffer(5u)]],
                                              uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
