// EXPECT: error the arguments of clamp have to be one type
// A float clamped between int literals is ambiguous to Apple as well.
kernel void math_clamp_with_int_bounds_rejected(
    device const float* f [[buffer(0)]],
    device float* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = clamp(f[i], 0, 1);
}
