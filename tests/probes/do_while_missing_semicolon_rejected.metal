// EXPECT: error after a do-while statement
kernel void do_while_missing_semicolon_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    do { out[i] = 1u; } while (false)
}
