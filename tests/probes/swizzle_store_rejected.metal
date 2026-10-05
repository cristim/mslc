// EXPECT: error assigning to ".xy" of a value that is not a struct, a swizzle, is not lowered yet
//
// Apple accepts a store to a swizzle; mslc lowers only reads of one.
kernel void swizzle_store_rejected(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.xy = b[i].zw;
    out[i] = v;
}
