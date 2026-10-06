// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_PhysicalStorageBuffer_uchar
//
// One lane of a bool vector in a buffer is a byte of its own.
kernel void bool_vector_element_store_in_buffer(device bool4 *flags [[buffer(0)]], device const uint *which [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    flags[i][which[i]] = true;
    flags[i].y = false;
}
