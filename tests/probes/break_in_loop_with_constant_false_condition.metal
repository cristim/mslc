// EXPECT: valid
kernel void break_in_loop_with_constant_false_condition(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 1u;
    while (false) { n = 2u; break; }
    out[i] = n;
}
