// EXPECT: valid
// DISASM: OpUDiv
// DISASM: OpUMod
// DISASM: OpShiftRightLogical
// DISASM-NOT: OpSDiv
// DISASM-NOT: OpSRem
// DISASM-NOT: OpShiftRightArithmetic
//
// A `u` suffix picks the unsigned opcode, for the three operators whose signed
// and unsigned forms are different instructions.
//
// **This probe passes on master, deliberately.** It is not a pre-change-failure
// probe and does not claim to be one. Master emits every integer literal as a
// %uint, which is right for this file and wrong for negative ones; the fix emits
// an int literal as an %int and a `u` one as a %uint, which is right for both.
// The intermediate state between the two, where every literal became an %int,
// broke this file's arithmetic: "4294967295u / 3u" became OpSDiv and answered 0
// instead of 1431655765, because the left operand's type picks the opcode.
//
// So what this holds is the regression that the obvious fix introduces. The
// negative literal is held by negative_int_literal instead, which does fail on
// master.
//
// So the suffix is what decides. A value that does not fit in an int with no
// suffix is a long in Apple's compiler and mslc cannot represent it, so both
// readings of that one are wrong: `float a = 4294967295;` reads as -1.0 here and
// as 4294967295.0 in Apple. That gap is separate, unfixed, and the comment in
// negative_int_literal says so rather than claiming this file settles it.
//
// The values are stored rather than dead so nothing folds them away, and the
// operators are all three: division and remainder read the sign of the left
// operand, and the right shift does too.
//
// The operands come from a buffer for the left one, so this is a real division by
// a real value rather than a constant the folder could answer, and the literals
// are on the right where their signedness does not choose the opcode.
kernel void unsigned_literal_picks_the_opcode(device int *out [[buffer(0)]],
                                             constant uint *in [[buffer(1)]],
                                             uint index [[thread_position_in_grid]])
{
    uint a = in[index];

    uint divided = a / 3u;
    uint remainder = a % 7u;
    uint shifted = a >> 1u;

    // And with the literal on the left, which is the case whose type picks the
    // opcode outright. Without these three the probe does not fail on the state it
    // exists to catch, because the regression moved the right-hand operand's type
    // too and a right-hand literal follows it.
    uint leftDivided = 4294967295u / a;
    uint leftRemainder = 4294967295u % a;
    uint leftShifted = 4294967295u >> a;

    out[0] = int(divided);
    out[1] = int(remainder);
    out[2] = int(shifted);
    out[3] = int(leftDivided);
    out[4] = int(leftRemainder);
    out[5] = int(leftShifted);
}