// EXPECT: valid
// DISASM-MATCH: = OpIEqual %bool
// DISASM-MATCH: = OpINotEqual %bool
// DISASM-MATCH: = OpSLessThan %bool
// DISASM-MATCH: = OpSLessThanEqual %bool
// DISASM-MATCH: = OpSGreaterThan %bool
// DISASM-MATCH: = OpSGreaterThanEqual %bool
// DISASM-NO-MATCH: = OpU(Less|Greater)\w* %bool
// DISASM-NO-MATCH: = OpFOrd\w* %bool
//
// All six comparison operators on two %int operands. This module holds only
// signed comparisons, so each needle can be satisfied only by the signed form,
// and the needles end at " %bool" so OpSLessThan cannot be satisfied by
// OpSLessThanEqual. At a = -1, b = 1 the signed and unsigned forms disagree, so
// an emitter that picked the wrong one would validate and compare wrongly.
// The operands come from a buffer so none of this is constant-folded.
kernel void comparison_signed(device int *out [[buffer(0)]],
                              constant int *in [[buffer(1)]],
                              uint index [[thread_position_in_grid]])
{
    int a = in[index];
    int b = in[index] - 1;

    if (a == b) { out[0] = 1; } else { out[0] = 0; }
    if (a != b) { out[1] = 1; } else { out[1] = 0; }
    if (a < b) { out[2] = 1; } else { out[2] = 0; }
    if (a <= b) { out[3] = 1; } else { out[3] = 0; }
    if (a > b) { out[4] = 1; } else { out[4] = 0; }
    if (a >= b) { out[5] = 1; } else { out[5] = 0; }
}
