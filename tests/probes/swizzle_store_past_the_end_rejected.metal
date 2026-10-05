// EXPECT: error ".zw" names a component past the end of a vector of 2
kernel void swizzle_store_past_the_end_rejected(device float2 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float2 v = float2(0.0);
    v.zw = b[i];
    out[i] = v;
}
