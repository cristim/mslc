// EXPECT: valid
// The true arm of the `(n & 1u) == 0u` test, which holds only the `continue`, branches to the continue block (which holds the condition, so
// the loop goes on or ends by it); the true arm of the `n > 40u` test, which holds only the `break`, branches to the merge.
// DISASM-CAPTURE: OpLoopMerge (%[0-9]+) (%[0-9]+) None
// DISASM-CAPTURE: OpIEqual %bool %[0-9]+ %uint_0[0-9_]*[^%A-Za-z]*OpSelectionMerge %[0-9]+ None[^%A-Za-z]*OpBranchConditional %[0-9]+ (%[0-9]+) %[0-9]+
// DISASM-CAPTURE: OpUGreaterThan %bool %[0-9]+ %uint_40[^%A-Za-z]*OpSelectionMerge %[0-9]+ None[^%A-Za-z]*OpBranchConditional %[0-9]+ (%[0-9]+) %[0-9]+
// DISASM-MATCH-CAPTURED: $3 = OpLabel[^%A-Za-z]*OpBranch $2[^%A-Za-z]*%[0-9]+ = OpLabel
// DISASM-MATCH-CAPTURED: $4 = OpLabel[^%A-Za-z]*OpBranch $1[^%A-Za-z]*%[0-9]+ = OpLabel
// DISASM-MATCH-CAPTURED: $2 = OpLabel[^%A-Za-z]*%[0-9]+ = OpLoad %uint %[0-9]+[^%A-Za-z]*%[0-9]+ = OpULessThan %bool %[0-9]+ %uint_9[^%A-Za-z]*OpBranchConditional
kernel void do_while_continue_and_break_land_on_the_condition_and_the_merge(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    uint s = 0u;
    do {
        n = n + 1u;
        if ((n & 1u) == 0u) continue;
        if (n > 40u) break;
        s = s + 1u;
    } while (n < 9u);
    out[i] = s * 100u + n;
}
