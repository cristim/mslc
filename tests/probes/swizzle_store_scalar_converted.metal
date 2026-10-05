// EXPECT: valid
// DISASM-MATCH: = OpCompositeInsert %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 1[^ _0-9a-zA-Z]
// DISASM-MATCH: = OpConvertSToF %float[^_0-9a-zA-Z]
//
// A scalar of another type is converted to the lane's, as in "v.y = 1".
kernel void swizzle_store_scalar_converted(device float4 *out [[buffer(0)]], constant int *n [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    float4 v = float4(0.0);
    v.y = n[i];
    out[i] = v;
}
