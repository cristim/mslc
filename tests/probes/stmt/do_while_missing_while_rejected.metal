// EXPECT: error expected "while" after the body of a do statement
kernel void do_while_missing_while_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    do { out[i] = 1u; }
    out[i] = 2u;
}
