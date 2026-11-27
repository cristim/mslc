// EXPECT: error function "transpose" is not a builtin mslc recognises
// transpose is a library call mslc does not lower.
kernel void matrix_transpose_rejected(device float4 *out [[buffer(0)]],
                                      device const float4x4 *m [[buffer(1)]],
                                      device const float4 *in [[buffer(2)]],
                                      uint i [[thread_position_in_grid]])
{
    out[i] = transpose(m[0]) * in[i];
}
