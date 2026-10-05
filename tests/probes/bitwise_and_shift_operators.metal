// EXPECT: valid
// DISASM-MATCH: = OpBitwiseAnd %uint
// DISASM-MATCH: = OpBitwiseOr %uint
// DISASM-MATCH: = OpBitwiseXor %uint
// DISASM-MATCH: = OpShiftLeftLogical %uint
// DISASM-MATCH: = OpNot %uint
// DISASM-MATCH: = OpLogicalNot %bool
//
// The bitwise operators, the left shift, and the two kinds of not. The right
// shifts are pinned by shift_right_by_sign, which is why they are not here.
//
// A left shift has no signed form in SPIR-V, so OpShiftLeftLogical is what any
// left shift has to be. ~ is OpNot on an integer and ! is OpLogicalNot on a
// bool; neither is a spelling of the other.
kernel void bitwise_and_shift_operators(device uint *out [[buffer(0)]],
                                         constant uint *in [[buffer(1)]],
                                         uint index [[thread_position_in_grid]])
{
    uint a = in[index];
    uint shift = 3u;

    out[0] = a & 0xffu;
    out[1] = a | 0xf0u;
    out[2] = a ^ 0x0fu;
    out[3] = a << shift;
    out[4] = ~a;

    bool flag = a > 4u;
    bool inverted = !flag;
    if (inverted) { out[5] = 1u; } else { out[5] = 0u; }
}
