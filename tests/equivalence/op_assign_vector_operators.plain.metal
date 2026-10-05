// float4 op= float4 and op= scalar are the plain operator.
kernel void vector_operators(device float4 *out [[buffer(0)]], device const float4 *in [[buffer(1)]],
                             uint i [[thread_position_in_grid]])
{
    float4 x = in[i];
    float4 y = in[i + 1u];
    x = x + y;
    x = x - y;
    x = x * y;
    x = x / y;
    x = x + 2.0f;
    x = x - 2.0f;
    x = x * 2.0f;
    x = x / 2.0f;
    out[i] = x;
}
