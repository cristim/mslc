// EXPECT: valid
// float4x4(1.0) is the identity: a diagonal matrix, a different lowering from the column form.
kernel void matrix_diagonal_construct(device float4 *out [[buffer(0)]],
                                      device const float4 *in [[buffer(1)]],
                                      uint i [[thread_position_in_grid]])
{
    float4x4 identity = float4x4(1.0); out[i] = identity * in[i];
}
