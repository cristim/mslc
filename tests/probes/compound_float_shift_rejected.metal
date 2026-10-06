// EXPECT: error operator <<= needs integer operands
//
// Apple: invalid operands to binary expression.
kernel void compound_float_shift_rejected(device float *out [[buffer(0)]],
                                          uint i [[thread_position_in_grid]])
{
    float x = out[i];
    x <<= 2;
    out[i] = x;
}
