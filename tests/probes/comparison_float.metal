// EXPECT: valid
// DISASM-MATCH: = OpFOrdEqual %bool
// DISASM-MATCH: = OpFOrdNotEqual %bool
// DISASM-MATCH: = OpFOrdLessThan %bool
// DISASM-MATCH: = OpFOrdLessThanEqual %bool
// DISASM-MATCH: = OpFOrdGreaterThan %bool
// DISASM-MATCH: = OpFOrdGreaterThanEqual %bool
// DISASM-NO-MATCH: = OpFUnord\w* %bool
// DISASM-NO-MATCH: = OpI\w* %bool
// DISASM-NO-MATCH: = OpS(Less|Greater)\w* %bool
// DISASM-NO-MATCH: = OpU(Less|Greater)\w* %bool
//
// All six comparison operators on two %float operands. The ordered FOrd forms
// are what a comparison against a NaN needs to be false, which is MSL's
// behaviour for everything but !=. Nothing here produces a NaN, so the pin is
// that mslc picked the ordered family, not that it handled a NaN.
kernel void comparison_float(device int *out [[buffer(0)]],
                             constant int *in [[buffer(1)]],
                             uint index [[thread_position_in_grid]])
{
    float a = float(in[index]);
    float b = float(in[index] - 1);

    if (a == b) { out[0] = 1; } else { out[0] = 0; }
    if (a != b) { out[1] = 1; } else { out[1] = 0; }
    if (a < b) { out[2] = 1; } else { out[2] = 0; }
    if (a <= b) { out[3] = 1; } else { out[3] = 0; }
    if (a > b) { out[4] = 1; } else { out[4] = 0; }
    if (a >= b) { out[5] = 1; } else { out[5] = 0; }
}
