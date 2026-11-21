// EXPECT: error a bool is not an index
//
// Apple: "invalid subexpression in attribute index expression".
kernel void attribute_index_bool_constant_rejected(device float *out [[buffer(true)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
