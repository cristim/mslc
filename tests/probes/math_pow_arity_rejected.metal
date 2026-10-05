// EXPECT: error pow takes 2 arguments, and this call passes 1
// A wrong argument count is reported, not padded.
kernel void math_pow_arity_rejected(
    device const float* f [[buffer(0)]],
    device float* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = pow(f[i]);
}
