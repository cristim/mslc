// EXPECT: error assigning to ".x" of "v", which is const or in constant memory
//
// Apple: cannot assign to variable 'v' with const-qualified type.
kernel void compound_swizzle_const_local_rejected(device float4 *out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    const float4 v = float4(0.0);
    v.x += 1.0;
    out[i] = v;
}
