// EXPECT: valid
// DISASM-MATCH: OpLoopMerge %[A-Za-z0-9_]+ %[A-Za-z0-9_]+ None
//
// Each clause of a for header may be empty; `for (;;)` loops until a break.
kernel void for_with_empty_header_clauses(device uint *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    for (;;) {
        n = n + 1u;
        if (n == 7u) break;
    }
    out[i] = n;
}
