// EXPECT: error cannot store through "v"
//
// Apple: cannot assign to variable 'v' with const-qualified type 'const float4'.
kernel void store_vector_element_to_const_local_rejected(device float *out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    const float4 v = float4(1.0);
    v[i & 3] = 2.0;
    out[0] = v.x;
}
