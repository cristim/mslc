// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpLabel[^%A-Za-z]*OpLoopMerge (%[0-9]+)
// DISASM-MATCH-CAPTURED: OpBranchConditional %[0-9A-Za-z_]+ $1 $2
// DISASM-ORDER: OpIAdd
// DISASM-ORDER: OpBranchConditional
kernel void do_while_false_condition_runs_once(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    do {
        n = n + 1u;
    } while (false);
    out[i] = n;
}
