// EXPECT: error is lowered only for numeric scalars, vectors and matrices
//
// A compound assignment on a bool is not lowered, in a buffer as in a local.
kernel void bool_buffer_compound_rejected(device bool *flags [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    flags[i] &= true;
}
