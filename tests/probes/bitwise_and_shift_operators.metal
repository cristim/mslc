// EXPECT: valid
// DISASM: OpBitwiseAnd
// DISASM: OpBitwiseOr
// DISASM: OpBitwiseXor
// DISASM: OpShiftLeftLogical
// DISASM: OpShiftRightArithmetic
// DISASM: OpShiftRightLogical
// DISASM: OpNot
// DISASM: OpLogicalNot
//
// The bitwise and shift operators, one line each, and nothing reached them
// before: the four of OpBitwiseAnd, OpBitwiseOr, OpBitwiseXor and
// OpShiftLeftLogical were in a dispatch whose only exercised entries were
// arithmetic.
//
// Two shifts on the same line, because the left shift has no signed form in
// SPIR-V and the right one does. OpShiftLeftLogical is the only left shift
// there is, so it is what a signed left shift has to be: shifting a negative
// %int left and reinterpreting is the same bit pattern, and the arithmetic right
// shift is the one that has a signed counterpart. The pins are that the
// arithmetic one is used where the operand is signed and the logical one where
// it is not, since the two differ for a negative value: -8 >> 1 is -4 signed
// and 2147483644 unsigned.
//
// ~ and ! are different opcodes on different types, OpNot on an integer and
// OpLogicalNot on a bool, and neither is a spelling of the other.
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

    // The same right shift, signed and unsigned, so both opcodes appear.
    int sa = int(a);
    out[4] = uint(sa >> 1);
    out[5] = uint(sa) >> 1u;

    // ~ is a bitwise complement, which is OpNot on an integer.
    out[6] = ~a;

    // ! on a bool is a different opcode, and the two being different is the
    // point: OpNot on a bool is not a thing the grammar allows.
    bool flag = a > 4u;
    bool inverted = !flag;
    if (inverted) { out[7] = 1u; } else { out[7] = 0u; }
}
