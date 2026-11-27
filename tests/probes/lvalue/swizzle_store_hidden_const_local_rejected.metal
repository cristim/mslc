// EXPECT: error assigning to ".xy" of "a", which is const or in constant memory
//
// The inner non-const "a" is gone when its block ends, so the store names the
// const one. Apple rejects it.
kernel void swizzle_store_hidden_const_local_rejected(device float4 *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    const float4 a = float4(0.0);
    {
        float4 a = float4(1.0);
        a.xy = float2(2.0);
        out[i] = a;
    }
    a.xy = float2(1.0);
}
