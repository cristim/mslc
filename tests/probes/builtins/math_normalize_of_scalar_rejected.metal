// EXPECT: error normalize takes vectors, and Metal has no scalar normalize
// Apple reports a scalar normalize as ambiguous, so mslc does not pick one.
kernel void math_normalize_of_scalar_rejected(
    device const float* f [[buffer(0)]],
    device float* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = normalize(f[i]);
}
