// EXPECT: error assigning to ".xyz" takes a vector of 3 components
//
// The value has to be as wide as the swizzle.
kernel void swizzle_store_too_few_rejected(device float4 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.xyz = b[i];
    out[i] = v;
}
