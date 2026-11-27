// EXPECT: error operator %= needs integer operands
//
// Apple: invalid operands to binary expression ('float' and 'float').
kernel void compound_float_remainder_rejected(device float *out [[buffer(0)]],
                                              uint i [[thread_position_in_grid]])
{
    float x = out[i];
    x %= 2.0f;
    out[i] = x;
}
