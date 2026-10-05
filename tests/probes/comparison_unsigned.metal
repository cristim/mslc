// EXPECT: valid
// DISASM-MATCH: = OpIEqual %bool
// DISASM-MATCH: = OpINotEqual %bool
// DISASM-MATCH: = OpULessThan %bool
// DISASM-MATCH: = OpULessThanEqual %bool
// DISASM-MATCH: = OpUGreaterThan %bool
// DISASM-MATCH: = OpUGreaterThanEqual %bool
// DISASM-NO-MATCH: = OpS(Less|Greater)\w* %bool
// DISASM-NO-MATCH: = OpFOrd\w* %bool
//
// All six comparison operators on two %uint operands. Only unsigned comparisons
// are in this module, so a signed opcode anywhere fails the NO-MATCH and a
// missing unsigned one fails its needle. See comparison_signed for the reason
// the pins end at " %bool".
kernel void comparison_unsigned(device uint *out [[buffer(0)]],
                                constant uint *in [[buffer(1)]],
                                uint index [[thread_position_in_grid]])
{
    uint a = in[index];
    uint b = in[index] - 1u;

    if (a == b) { out[0] = 1u; } else { out[0] = 0u; }
    if (a != b) { out[1] = 1u; } else { out[1] = 0u; }
    if (a < b) { out[2] = 1u; } else { out[2] = 0u; }
    if (a <= b) { out[3] = 1u; } else { out[3] = 0u; }
    if (a > b) { out[4] = 1u; } else { out[4] = 0u; }
    if (a >= b) { out[5] = 1u; } else { out[5] = 0u; }
}
