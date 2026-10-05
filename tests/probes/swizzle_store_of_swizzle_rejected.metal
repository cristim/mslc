// EXPECT: error assigning to ".x" of a swizzle is not lowered yet
//
// Apple accepts a store through "v.xyz.x"; mslc does not lower it.
kernel void swizzle_store_of_swizzle_rejected(device float4 *out [[buffer(0)]], constant float *s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.xyz.x = s[i];
    out[i] = v;
}
