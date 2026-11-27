// EXPECT: error ".xyz" names a component past the end of a vector of 2
//
// Apple reports "vector component access exceeds type 'float2'".
kernel void swizzle_past_the_end_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(float2(a[i].x).xyz, 1.0);
}
