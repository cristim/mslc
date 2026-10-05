// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v2float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 4 5 2 3[^ _0-9a-zA-Z]
//
// A scalar stored to a several-lane swizzle goes to every lane it names.
kernel void swizzle_store_scalar_broadcast(device float4 *out [[buffer(0)]], constant float *s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.xy = s[i];
    out[i] = v;
}
