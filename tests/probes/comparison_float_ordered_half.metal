// EXPECT: valid
// DISASM-MATCH: = OpFOrdEqual %bool
// DISASM-MATCH: = OpFOrdLessThan %bool
// DISASM-MATCH: = OpFOrdLessThanEqual %bool
// DISASM-MATCH: = OpFOrdGreaterThan %bool
// DISASM-MATCH: = OpFOrdGreaterThanEqual %bool
// DISASM-NO-MATCH: = OpFUnord\w* %bool
// DISASM-NO-MATCH: = OpFOrdNotEqual %bool
//
// The other five operators on %half operands stay ordered, so each is false
// when either operand is NaN.
kernel void comparison_float_ordered_half(device int *out [[buffer(0)]],
                                          device const half *in [[buffer(1)]],
                                          uint index [[thread_position_in_grid]])
{
    half a = in[index];
    half b = in[index + 1];

    if (a == b) { out[0] = 1; } else { out[0] = 0; }
    if (a < b) { out[1] = 1; } else { out[1] = 0; }
    if (a <= b) { out[2] = 1; } else { out[2] = 0; }
    if (a > b) { out[3] = 1; } else { out[3] = 0; }
    if (a >= b) { out[4] = 1; } else { out[4] = 0; }
}
