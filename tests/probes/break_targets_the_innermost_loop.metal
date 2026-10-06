// EXPECT: valid
kernel void break_targets_the_innermost_loop(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint sum = 0u;
    for (uint a = 0u; a < 5u; a = a + 1u) {
        for (uint b = 0u; b < 5u; b = b + 1u) {
            if (b > a) break;
            sum = sum + 1u;
        }
        if (a == 2u) continue;
        sum = sum + 100u;
    }
    out[i] = sum;
}
