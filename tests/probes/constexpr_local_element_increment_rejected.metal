// EXPECT: error cannot store through "v"
//
// Apple: "cannot assign to variable 'v' with const-qualified type".
kernel void constexpr_local_element_increment_rejected(device float *out [[buffer(0)]])
{
    constexpr float4 v = float4(1.0);
    v[1]++;
    out[0] = v.x;
}
