// m *= s and v *= m are the plain products.
kernel void matrix_scale(device float4x4 *out [[buffer(0)]], device const float4 *in [[buffer(1)]],
                         uint i [[thread_position_in_grid]])
{
    float4x4 m = out[i];
    float4 v = in[i];
    m *= 2.0f;
    v *= m;
    out[i] = m;
    out[i + 1u] = float4x4(v, v, v, v);
}
