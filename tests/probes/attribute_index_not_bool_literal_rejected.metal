// EXPECT: error a bool is not an index
kernel void attribute_index_not_bool_literal_rejected(device float *out [[buffer(!false)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
