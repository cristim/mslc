// EXPECT: valid
// DISASM-MATCH: = OpFMul %float %[0-9]+ %float_2
// DISASM-NO-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_float .*OpAccessChain %_ptr_PhysicalStorageBuffer_float 
//
// A member of a buffer element: one address, one load, one store.
struct Pair
{
    float a;
    float b;
};

kernel void compound_struct_member_in_a_buffer(device Pair *out [[buffer(0)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i].b *= 2.0f;
}
