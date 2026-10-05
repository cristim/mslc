// EXPECT: valid
// DISASM-MATCH: = OpVectorShuffle %v2int %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ 3 2[^ _0-9a-zA-Z]
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Aligned 8[^ _0-9a-zA-Z]
//
// A member of a buffer's struct element is stored through the buffer pointer.
struct Record {
    int2 id;
    float4 colour;
};

kernel void swizzle_store_buffer_struct_member(device Record *out [[buffer(0)]], constant int2 *b [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i].id.yx = b[i];
}
