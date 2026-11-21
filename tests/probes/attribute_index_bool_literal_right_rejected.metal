// EXPECT: error a bool is not an index
kernel void attribute_index_bool_literal_right_rejected(device float *out [[buffer(1 + true)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
