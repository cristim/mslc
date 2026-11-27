// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpLabel[^%A-Za-z]*OpLoopMerge (%[0-9]+)
// DISASM-MATCH-CAPTURED: OpBranchConditional %[0-9A-Za-z_]+ $1 $2
// DISASM-LOOP-MERGE-REACHED: yes
kernel void do_while_counting_loop(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    uint sum = 0u;
    do {
        sum = sum + n;
        n = n + 1u;
    } while (n < 10u && sum < 100u);
    out[i] = sum;
}
