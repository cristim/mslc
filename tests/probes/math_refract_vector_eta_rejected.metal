// EXPECT: error refract's third argument is the scalar ratio
// eta is a scalar in Metal and in GLSL.std.450 Refract.
kernel void math_refract_vector_eta_rejected(
    device const float3* v [[buffer(0)]],
    device float3* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = refract(v[i], v[i + 1u], v[i + 2u]);
}
