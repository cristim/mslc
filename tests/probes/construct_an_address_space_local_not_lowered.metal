// EXPECT: error a threadgroup local "v" with an initialiser is not lowered yet
//
// "threadgroup float3 v(0);" is legal MSL that xcrun metal accepts. A threadgroup local
// is now a Workgroup variable, and its initialiser is refused because the source does
// not say which invocation runs it. "device float3 v(0);" is not a threadgroup local
// and keeps the address-space refusal.
kernel void construct_an_address_space_local_not_lowered(
    device float3 *out [[buffer(0)]],
    uint index [[thread_position_in_grid]])
{
    threadgroup float3 v(0);
    out[index] = v;
}
