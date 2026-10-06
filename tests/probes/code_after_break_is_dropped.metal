// EXPECT: valid
// Code after a break or continue in the same block is unreachable and not emitted.
kernel void code_after_break_is_dropped(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    for (uint j = 0u; j < 4u; j = j + 1u) {
        n = n + 1u;
        if (j == 1u) { break; n = 99u; }
        continue;
        n = 77u;
    }
    out[i] = n;
}
