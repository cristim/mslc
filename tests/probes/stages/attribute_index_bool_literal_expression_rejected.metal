// EXPECT: error a bool is not an index
//
// Apple: "invalid subexpression in attribute index expression" for true + 1 and !false.
kernel void attribute_index_bool_literal_expression_rejected(device float *out [[buffer(true + 1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
