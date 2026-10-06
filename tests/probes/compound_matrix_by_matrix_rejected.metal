// EXPECT: error operator *= takes a scalar on the right of a matrix
//
// Apple: no viable overloaded '*='. A product of two matrices is written m = m * n.
kernel void compound_matrix_by_matrix_rejected(device float4x4 *out [[buffer(0)]], device const float4x4 *m [[buffer(1)]],
                                               uint i [[thread_position_in_grid]])
{
    float4x4 x = out[i];
    x *= m[i];
    out[i] = x;
}
