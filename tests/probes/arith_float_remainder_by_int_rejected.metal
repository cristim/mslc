// EXPECT: error operator % needs integer operands
//
// A float on the left of % is rejected as well as one on the right: Apple takes
// fmod for floating point, whichever side the float is on.
kernel void arith_float_remainder_by_int_rejected(device float *out [[buffer(0)]],
                                                  device const float *a [[buffer(1)]],
                                                  device const int *b [[buffer(2)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = a[i] % b[i];
}
