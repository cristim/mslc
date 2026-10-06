// EXPECT: error constant address space
//
// A store into a constant buffer is rejected for a bool as for any other type.
kernel void bool_constant_buffer_store_rejected(constant bool *flags [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    flags[i] = true;
}
