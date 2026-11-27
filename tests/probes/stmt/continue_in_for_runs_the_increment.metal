// EXPECT: valid
// continue in a for branches to the continue block holding the increment; values are checked by read-back.
kernel void continue_in_for_runs_the_increment(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint sum = 0u;
    for (uint j = 0u; j < 10u; j = j + 1u) {
        if ((j & 1u) == 1u) {
            continue;
        }
        sum = sum + j;
    }
    out[i] = sum;
}
