// EXPECT: error a unary operator on a matrix is not lowered yet
// OpFNegate takes no matrix, and Apple rejects unary minus on one.
kernel void matrix_negate_rejected(device float4 *out [[buffer(0)]],
                                   device const float4x4 *m [[buffer(1)]],
                                   device const float4 *in [[buffer(2)]],
                                   uint i [[thread_position_in_grid]])
{
    out[i] = in[i]; float4x4 n = -m[0];
}
