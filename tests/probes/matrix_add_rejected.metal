// EXPECT: error only * is lowered with a matrix operand
// Metal adds two matrices element by element; mslc lowers only the products.
kernel void matrix_add_rejected(device float4 *out [[buffer(0)]],
                                device const float4x4 *m [[buffer(1)]],
                                device const float4 *in [[buffer(2)]],
                                uint i [[thread_position_in_grid]])
{
    out[i] = (m[0] + m[1]) * in[i];
}
