// EXPECT: valid
// DISASM: OpSLessThan %bool
// DISASM: OpBitcast %uint
// DISASM: OpSConvert %short
// Refs #391: "(" followed by a single-word integer functional cast such as "int(2)" opens a
// parenthesised expression rather than an unfinished C-style cast.
#include <metal_stdlib>
using namespace metal;

kernel void cast_parenthesised_int_functional_cast(device uint* out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    int n = int(i);
    while (!(int(2) < n)) {
        n = n + 1;
    }
    out[i] = (unsigned(n) + 1u) + uint((short(n) - short(1)));
}
