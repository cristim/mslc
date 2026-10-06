// EXPECT: error operator & needs integer operands
kernel void arith_float_bitand_rejected(device float *out [[buffer(0)]],
                                        device const float *a [[buffer(1)]],
                                        device const int *b [[buffer(2)]],
                                        uint i [[thread_position_in_grid]])
{
    out[i] = a[i] & b[i];
}
