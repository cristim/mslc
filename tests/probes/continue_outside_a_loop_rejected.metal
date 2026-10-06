// EXPECT: error 'continue' statement not in loop statement
kernel void continue_outside_a_loop_rejected(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    if (out[i] == 0u) continue;
    out[i] = 1u;
}
