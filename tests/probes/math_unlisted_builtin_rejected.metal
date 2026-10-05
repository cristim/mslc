// EXPECT: error function "sinh" is not a builtin mslc recognises
// A builtin outside the table stays reported.
kernel void math_unlisted_builtin_rejected(
    device const float* f [[buffer(0)]],
    device float* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = sinh(f[i]);
}
