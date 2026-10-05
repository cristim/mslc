// EXPECT: valid
// DISASM-MATCH: = OpINotEqual %bool %[_0-9a-zA-Z]+ %uint_0
// DISASM-MATCH: OpLoopMerge
//
// The same for a for loop whose condition is an unsigned integer.
kernel void for_condition_from_int(device uint *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    uint n = 0u;
    for (uint k = v[i]; k; k = k - 1u) {
        n = n + 1u;
    }
    out[i] = n;
}
