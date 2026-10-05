// EXPECT: error a matrix is scaled by a scalar of its own component type
// Apple's compiler rejects a float4x4 times an int rather than converting it.
kernel void matrix_scaled_by_int_rejected(device float4 *out [[buffer(0)]],
                                          device const float4x4 *m [[buffer(1)]],
                                          device const float4 *in [[buffer(2)]],
                                          uint i [[thread_position_in_grid]])
{
    out[i] = (m[0] * 2) * in[i];
}
