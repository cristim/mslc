// EXPECT: error a long is not an index

//
// Apple takes an int or a uint as an attribute index and rejects a long: invalid type
// 'long' for attribute index expression.
kernel void attribute_index_long_suffix_rejected(device uint* out [[buffer(5l)]],
                                                 uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
