// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float[^_0-9a-zA-Z]
// DISASM-MATCH: = OpCompositeConstruct %v2float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
//
// A scalar of another type is converted once, then put in every lane.
kernel void swizzle_store_scalar_broadcast_converted(device float4 *out [[buffer(0)]], constant int *n [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.zw = n[i];
    out[i] = v;
}
