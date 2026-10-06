// EXPECT: valid
// The if merge block has no predecessor.
kernel void both_arms_leave_the_loop(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    for (uint j = 0u; j < 6u; j = j + 1u) {
        n = n + 1u;
        if (j == 3u) break; else continue;
    }
    out[i] = n;
}
