// EXPECT: error ".xyzwx" names more than four components
//
// Apple reports "vector component access has invalid length 5".
kernel void swizzle_too_long_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = a[i].xyzwx;
}
