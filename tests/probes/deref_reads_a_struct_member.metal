// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_float %[0-9]+ %uint_0(_[0-9]+)? %int_0(_[0-9]+)? %uint_1(_[0-9]+)?
// DISASM-MATCH: OpLoad %float %[0-9]+ Aligned 4
//
// "(*p).b" is the member of the first element, the same chain "p->b" and
// "p[0].b" build: into the array, element zero, then the field.
struct Pair {
    float4 a;
    float b;
};

kernel void deref_reads_a_struct_member(device float* out [[buffer(0)]],
                                        device const Pair* in [[buffer(1)]])
{
    out[0] = (*in).b;
}
