// EXPECT: error a long is not an index

//
// The type of the sum is long.
kernel void attribute_index_long_in_a_sum_rejected(device uint* out [[buffer(1 + 4l)]],
                                                   uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
