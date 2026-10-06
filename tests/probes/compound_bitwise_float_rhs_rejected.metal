// EXPECT: error operator &= needs integer operands
//
// Apple: invalid operands to binary expression ('int' and 'float').
kernel void compound_bitwise_float_rhs_rejected(device int *out [[buffer(0)]], device const float *f [[buffer(1)]],
                                                uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x &= f[i];
    out[i] = x;
}
