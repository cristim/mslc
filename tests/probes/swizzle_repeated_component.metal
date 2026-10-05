// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v2float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 0 0[^ _0-9a-zA-Z]
//
// A read may name one component twice.
kernel void swizzle_repeated_component(device float4 *out [[buffer(0)]], constant float4 *a [[buffer(1)]], constant float4 *b [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4((a[i] + b[i]).xx, 0.0, 1.0);
}
