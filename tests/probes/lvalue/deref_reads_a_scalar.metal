// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_float %[0-9]+ %uint_0(_[0-9]+)? %int_0(_[0-9]+)?
// DISASM-MATCH: OpLoad %float %[0-9]+ Aligned 4
//
// "*p" reads the first element, the way "p[0]" does: an access chain into the
// buffer's array at element zero, and a load of the element. tests/equivalence
// holds the same program in both spellings and requires the same module.
kernel void deref_reads_a_scalar(device float* out [[buffer(0)]],
                                 device const float* in [[buffer(1)]])
{
    out[0] = *in;
}
