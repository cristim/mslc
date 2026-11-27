// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v2int %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 3 2[^ _0-9a-zA-Z]
//
// An integer vector keeps its component type.
kernel void swizzle_store_int_vector(device int2 *out [[buffer(0)]], constant int2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i].yx = b[i];
}
