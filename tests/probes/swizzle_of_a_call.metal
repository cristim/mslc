// EXPECT: valid
// DISASM-MATCH: = OpCompositeExtract %float %[_0-9a-zA-Z]+ 3[^ _0-9a-zA-Z]
// DISASM-NOT: OpVectorShuffle
//
// One component of a call result is an extract, not a one-lane shuffle.
kernel void swizzle_of_a_call(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(normalize(a[i]).w);
}
