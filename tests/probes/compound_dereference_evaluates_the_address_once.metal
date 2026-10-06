// EXPECT: valid
// DISASM-NO-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_int .*OpAccessChain %_ptr_PhysicalStorageBuffer_int 
//
// *p += 5 computes the element's address once.
kernel void compound_dereference_evaluates_the_address_once(device int *p [[buffer(0)]],
                                                            uint i [[thread_position_in_grid]])
{
    *p += 5;
}
