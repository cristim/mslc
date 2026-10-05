// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v4half %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 0 4 2 5[^ _0-9a-zA-Z]
//
// A half vector keeps its component type, and its stride is 8 bytes.
kernel void swizzle_store_half_vector(device half4 *out [[buffer(0)]], constant half2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i].yw = b[i];
}
