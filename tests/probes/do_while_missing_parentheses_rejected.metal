// EXPECT: error expected ( after 'while'
kernel void do_while_missing_parentheses_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    do { out[i] = 1u; } while false;
}
