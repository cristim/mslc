// EXPECT: error assigning to ".xx" names a component twice
//
// Apple rejects "vector is not assignable (contains duplicate components)".
kernel void swizzle_store_repeated_rejected(device float4 *out [[buffer(0)]], constant float2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.xx = b[i];
    out[i] = v;
}
