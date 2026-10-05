// EXPECT: error ".xg" is not a swizzle
//
// Apple reports "illegal vector component name 'g'".
kernel void swizzle_mixed_letter_sets_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(a[i].xg, 0.0, 0.0);
}
