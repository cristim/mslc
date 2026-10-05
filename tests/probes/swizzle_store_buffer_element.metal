// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 5 4 2 3[^ _0-9a-zA-Z]
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Aligned 16[^ _0-9a-zA-Z]
//
// The element is loaded, its lanes replaced, and the whole vector stored back
// through the buffer pointer.
kernel void swizzle_store_buffer_element(device float4 *out [[buffer(0)]], constant float4 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i].yx = b[i].xy;
}
