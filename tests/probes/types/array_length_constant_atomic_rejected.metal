// EXPECT: error a local atomic "N" is not allowed
kernel void array_length_constant_atomic_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    const atomic_int N = 3;
    float a[N];
    out[i] = a[0];
}
