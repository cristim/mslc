// EXPECT: error only * is lowered with a matrix operand
//
// Apple rejects a scalar sum and accepts matrix += matrix; neither is lowered, as for the plain operator.
kernel void compound_matrix_sum_not_lowered(device float4x4 *out [[buffer(0)]], device const float4x4 *m [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{
    float4x4 x = out[i];
    x += 2.0f;
    out[i] = x;
}
