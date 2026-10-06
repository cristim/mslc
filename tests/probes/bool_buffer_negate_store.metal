// EXPECT: valid
// DISASM-MATCH: OpLogicalNot %bool
//
// A bool element read, negated and written back.
kernel void bool_buffer_negate_store(device bool *flags [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    flags[i] = !flags[i];
}
