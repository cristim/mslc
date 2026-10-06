// EXPECT: error 'break' statement not in loop or switch statement
kernel void break_outside_a_loop_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    out[i] = 1u;
    break;
}
