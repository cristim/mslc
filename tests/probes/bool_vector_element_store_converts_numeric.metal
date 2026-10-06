// EXPECT: valid
// DISASM-MATCH: OpINotEqual %bool %[0-9]+ %uint_0
// DISASM-MATCH: OpSelect %uchar
//
// A number stored to a lane of a bool vector in a buffer is true when it is not
// zero, and the byte written is 1: 5 does not land in the buffer as 5.
kernel void bool_vector_element_store_converts_numeric(device bool4 *flags [[buffer(0)]], device const uint *in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    flags[i][2] = in[i];
}
