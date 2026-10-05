// EXPECT: error cross takes three-component vectors, and these have 4
// GLSL.std.450 Cross and Metal's cross are both three-component only.
kernel void math_cross_of_float4_rejected(
    device const float4* v [[buffer(0)]],
    device float4* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = cross(v[i], v[i + 1u]);
}
