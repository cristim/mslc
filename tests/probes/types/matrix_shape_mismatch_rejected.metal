// EXPECT: error the operands of a matrix product do not match
// A float4x4 times a float3 has no product; Apple rejects it too.
kernel void matrix_shape_mismatch_rejected(device float4 *out [[buffer(0)]],
                                           device const float4x4 *m [[buffer(1)]],
                                           device const float4 *in [[buffer(2)]],
                                           uint i [[thread_position_in_grid]])
{
    float3 short_vector = float3(1.0); out[i] = m[0] * short_vector;
}
