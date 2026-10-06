// EXPECT: valid
kernel void break_in_conditionless_for(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    for (uint j = 0u;; j = j + 1u) {
        n = n + 1u;
        if (n == 7u) break;
    }
    out[i] = n;
}
