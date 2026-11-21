// EXPECT: valid
// With no continue statement, the end of the body branches to the continue block, which holds the
// condition, and the back edge to the loop header is that block's conditional branch (false goes
// to the merge): the body runs before the condition is tested, so it runs once when it starts false (the read-back checks the count).
// DISASM-CAPTURE: (%[0-9]+) = OpLabel[^%A-Za-z]*OpLoopMerge (%[0-9]+) (%[0-9]+) None
// DISASM-MATCH-CAPTURED: OpIAdd %uint %[0-9]+ %uint_1[^%A-Za-z]*OpStore %[0-9]+ %[0-9]+[^%A-Za-z]*OpBranch $3[^%A-Za-z]*$3 = OpLabel[^%A-Za-z]*%[0-9]+ = OpLoad %uint %[0-9]+[^%A-Za-z]*%[0-9]+ = OpULessThan %bool %[0-9]+ %uint_5[^%A-Za-z]*OpBranchConditional %[0-9]+ $1 $2
kernel void do_while_body_falls_into_the_condition_block(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    do {
        n = n + 1u;
    } while (n < 5u);
    out[i] = n;
}
