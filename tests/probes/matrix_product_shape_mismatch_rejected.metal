// EXPECT: error the operands of a matrix product do not match
// A float2x3 has two columns and three rows, so it cannot multiply another
// float2x3; Apple rejects the product too.
kernel void matrix_product_shape_mismatch_rejected(device float3 *out [[buffer(0)]],
                                                   device const float2x3 *m [[buffer(1)]],
                                                   device const float2 *in [[buffer(2)]],
                                                   uint i [[thread_position_in_grid]])
{
    float2x3 product = m[0] * m[1];
    out[i] = product * in[i];
}
