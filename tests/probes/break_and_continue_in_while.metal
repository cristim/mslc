// EXPECT: valid
kernel void break_and_continue_in_while(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    uint sum = 0u;
    while (true) {
        n = n + 1u;
        if (n > 20u) break;
        if ((n % 3u) == 0u) continue;
        sum = sum + n;
    }
    out[i] = sum;
}
