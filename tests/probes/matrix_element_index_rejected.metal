// EXPECT: error indexing an expression is not supported yet
// m[c][r] reaches one element through a column; only the column is lowered.
kernel void matrix_element_index_rejected(device float4 *out [[buffer(0)]],
                                          device const float4x4 *m [[buffer(1)]],
                                          device const float4 *in [[buffer(2)]],
                                          uint i [[thread_position_in_grid]])
{
    float4x4 local = m[0]; out[i] = float4(local[1][2]);
}
