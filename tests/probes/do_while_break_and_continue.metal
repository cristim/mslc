// EXPECT: valid
// DISASM-LOOP-MERGE-REACHED: yes
kernel void do_while_break_and_continue(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    uint sum = 0u;
    do {
        n = n + 1u;
        if (n > 20u) break;
        if ((n % 3u) == 0u) continue;
        sum = sum + n;
    } while (n < 50u);
    out[i] = sum;
}
