// EXPECT: error normalize takes float or half vectors
// Metal's normalize is floating point only, and GLSL.std.450 Normalize too.
kernel void math_normalize_of_integer_rejected(
    device const int3* v [[buffer(0)]],
    device float3* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = normalize(v[i]);
}
