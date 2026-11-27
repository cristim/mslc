// EXPECT: valid
// Add, sub and mul are sign-agnostic in SPIR-V, so these are safe to pin now.
kernel void uint_mul_add_sub(device const uint* in [[buffer(0)]],
                             device uint* out [[buffer(1)]],
                             uint i [[thread_position_in_grid]])
{ out[i] = in[i] * 3u + i - 1u; }
