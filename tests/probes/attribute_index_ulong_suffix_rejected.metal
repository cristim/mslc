// EXPECT: error a long is not an index

//
// A ulong is rejected too.
kernel void attribute_index_ulong_suffix_rejected(device uint* out [[buffer(5ul)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
