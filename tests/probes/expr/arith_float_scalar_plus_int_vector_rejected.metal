// EXPECT: error floating-point scalar cannot be combined with an integer vector
//
// Apple rejects float + int2 as it rejects int2 + float; the scalar on the left
// takes the same rule as the scalar on the right.
kernel void arith_float_scalar_plus_int_vector_rejected(device int2 *out [[buffer(0)]],
                                                        device const float *f [[buffer(1)]],
                                                        device const int2 *in [[buffer(2)]],
                                                        uint i [[thread_position_in_grid]])
{
    out[i] = f[i] + in[i];
}
