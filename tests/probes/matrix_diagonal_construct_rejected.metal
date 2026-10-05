// EXPECT: error float4x4 built from 1 values is not lowered yet
// float4x4(1.0) is a diagonal matrix, a different lowering from the column form.
kernel void matrix_diagonal_construct_rejected(device float4 *out [[buffer(0)]],
                                               device const float4x4 *m [[buffer(1)]],
                                               device const float4 *in [[buffer(2)]],
                                               uint i [[thread_position_in_grid]])
{
    float4x4 identity = float4x4(1.0); out[i] = identity * in[i];
}
