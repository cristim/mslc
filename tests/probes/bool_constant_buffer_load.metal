// EXPECT: valid
// DISASM-MATCH: OpINotEqual %bool
//
// A constant buffer of bools is read like a device one.
kernel void bool_constant_buffer_load(device uint *out [[buffer(0)]], constant bool *flags [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = uint(flags[i]);
}
