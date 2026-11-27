// EXPECT: error the arguments of mix have to be one type
// A float weight beside half operands is ambiguous to Apple as well.
kernel void math_mix_of_half_and_float_rejected(
    device const half* h [[buffer(0)]],
    device half* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = mix(h[i], h[i + 1u], 0.5f);
}
