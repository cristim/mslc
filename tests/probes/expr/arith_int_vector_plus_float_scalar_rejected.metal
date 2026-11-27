// EXPECT: error floating-point scalar cannot be combined with an integer vector
//
// Apple rejects int2 + float; mslc converted the float to int and added that.
kernel void arith_int_vector_plus_float_scalar_rejected(device int2 *out [[buffer(0)]],
                                                        device const int2 *in [[buffer(1)]],
                                                        device const float *f [[buffer(2)]],
                                                        uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + f[i];
}
