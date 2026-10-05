// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_float %[0-9]+ %uint_0(_[0-9]+)? %int_0(_[0-9]+)? %uint_1(_[0-9]+)?
// DISASM-MATCH: OpStore %[0-9]+ %float_2 Aligned 4
//
struct Pair {
    float4 a;
    float b;
};

kernel void deref_writes_a_struct_member(device Pair* out [[buffer(0)]])
{
    (*out).b = 2.0;
}
