// EXPECT: error assigning to ".x" of "a", which is const or in constant memory
//
// A name declared in a for header stops naming that variable after the loop.
kernel void swizzle_store_after_for_shadow(device float4 *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    const float4 a = float4(0.0);
    for (float4 a = float4(1.0); a.x < 3.0; a = a + float4(1.0)) {
        out[i] = a;
    }
    a.x = 1.0;
}
