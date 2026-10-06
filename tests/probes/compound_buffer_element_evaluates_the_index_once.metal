// EXPECT: valid
// DISASM-MATCH: = OpIMul %uint %[0-9]+ %uint_7
// DISASM-NO-MATCH: OpIMul %uint %[0-9]+ %uint_7.*OpIMul %uint %[0-9]+ %uint_7
// DISASM-NO-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_int .*OpAccessChain %_ptr_PhysicalStorageBuffer_int 
//
// out[i * 7u] += 1 computes the element's address once and loads and stores through it; the
// index expression is not evaluated for the load and again for the store.
kernel void compound_buffer_element_evaluates_the_index_once(device int *out [[buffer(0)]],
                                                             uint i [[thread_position_in_grid]])
{
    out[i * 7u] += 1;
}
