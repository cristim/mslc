// EXPECT: error zero-length arrays are not permitted
constant int N = 0;
kernel void array_length_named_constant_zero_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
