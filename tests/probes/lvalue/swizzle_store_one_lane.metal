// EXPECT: valid
// DISASM-MATCH: = OpCompositeInsert %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 3[^ _0-9a-zA-Z]
//
// A single-component swizzle takes a scalar and replaces one lane.
kernel void swizzle_store_one_lane(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float *s [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    float4 v = a[i];
    v.w = s[i];
    out[i] = v;
}
