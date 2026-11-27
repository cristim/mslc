// EXPECT: valid
kernel void float_mul_sub_div(device const float* in [[buffer(0)]],
                              device float* out [[buffer(1)]],
                              uint i [[thread_position_in_grid]])
{ out[i] = (in[i] * 2.0 - 1.0) / 4.0; }
