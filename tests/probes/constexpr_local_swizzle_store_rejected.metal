// EXPECT: error assigning to ".xy" of "v", which is const or in constant memory
//
// Apple: "cannot assign to variable 'v' with const-qualified type".
kernel void constexpr_local_swizzle_store_rejected(device float4 *out [[buffer(0)]])
{
    constexpr float4 v = float4(1.0);
    v.xy = float2(2.0);
    out[0] = v;
}
