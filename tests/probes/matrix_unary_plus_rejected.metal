// EXPECT: error a unary operator on a matrix is not lowered yet
// Apple rejects unary plus on a matrix as well as minus, so mslc does too.
kernel void matrix_unary_plus_rejected(device float4 *out [[buffer(0)]],
                                       device const float4x4 *m [[buffer(1)]],
                                       uint i [[thread_position_in_grid]])
{
    float4x4 same = +m[0];
    out[i] = same[0];
}
