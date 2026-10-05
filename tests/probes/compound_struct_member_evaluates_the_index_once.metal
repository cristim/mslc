// EXPECT: valid
// DISASM-MATCH: = OpIAdd %uint %[0-9]+ %uint_1
// DISASM-NO-MATCH: OpIAdd %uint %[0-9]+ %uint_1.*OpIAdd %uint %[0-9]+ %uint_1
// DISASM-NO-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_int .*OpAccessChain %_ptr_PhysicalStorageBuffer_int 
//
// s[idx[i] + 1u].w ^= 5 loads idx[i] once, and computes the member's address once.
struct Item
{
    int w;
    int z;
};

kernel void compound_struct_member_evaluates_the_index_once(device Item *s [[buffer(0)]], device const uint *idx [[buffer(1)]],
                                                            uint i [[thread_position_in_grid]])
{
    s[idx[i] + 1u].w ^= 5;
}
