// EXPECT: error ".xg" is not a swizzle: each letter has to be one of xyzw, or each one of rgba
kernel void swizzle_store_mixed_letter_sets_rejected(device float4 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.xg = b[i];
    out[i] = v;
}
