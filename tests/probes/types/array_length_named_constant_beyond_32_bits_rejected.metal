// EXPECT: error between 0 and 4294967295
constant long N = 4294967296;
kernel void array_length_named_constant_beyond_32_bits_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
