// EXPECT: valid
kernel void break_in_deeply_nested_ifs(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    while (n < 50u) {
        n = n + 1u;
        if (n > 2u) {
            if (n > 4u) {
                if (n > 6u) {
                    if (n == 9u) { break; }
                }
            }
        }
    }
    out[i] = n;
}
