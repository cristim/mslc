// EXPECT: error vectors of different widths
//
// int2 << int4 has no lowering: the count needs as many lanes as the value.
kernel void arith_shift_vector_widths_differ_rejected(device int2 *out [[buffer(0)]],
                                                      device const int2 *a [[buffer(1)]],
                                                      device const int4 *n [[buffer(2)]],
                                                      uint i [[thread_position_in_grid]])
{
    out[i] = a[i] << n[i];
}
