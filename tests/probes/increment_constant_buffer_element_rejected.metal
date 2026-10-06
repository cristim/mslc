// EXPECT: error cannot store through "p"
//
// Apple: read-only variable is not assignable.
kernel void increment_constant_buffer_element_rejected(device float *out [[buffer(0)]],
                                                   constant int *p [[buffer(1)]])
{
    p[1]++;
    out[0] = 1.0;
}
