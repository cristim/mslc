// EXPECT: valid
// DISASM: OpULessThan
// DISASM: OpULessThanEqual
// DISASM: OpUGreaterThan
// DISASM: OpUGreaterThanEqual
// DISASM-NOT: OpSLessThan
// DISASM-NOT: OpSGreaterThan
//
// A signed and an unsigned operand in one comparison, all six operators.
//
// MSL's rule is C's: a signed and an unsigned integer of the same width both
// become unsigned. So with s = -1 and u = 1, "s < u" is false and "s > u" is
// true, because s converts to 4294967295 first. Emit OpSLessThan over the %int
// and the %uint and every one of the six answers the other way.
//
// The emitter did not convert its comparison operands at all before this. It took
// the signedness of the left one and passed the right through unchanged, so an
// int against a uint was signed whatever the source said, and the instruction
// itself was mixed-type: OpSLessThan with an %int and a %uint operand. That is
// pre-existing on master and not caused by any literal change, and it validates,
// which is what made it worth finding.
//
// The NOT pins are the claim. A left operand that is already a %int is the case
// that was wrong, so the module has to carry the unsigned opcodes and none of the
// signed ones: the conversion moves the signed side rather than the unsigned one.
//
// The same width is what makes it a conversion and not a promotion: an int beside
// a long is not this rule, and mslc has no 64-bit literal to reach it with.
kernel void mixed_signedness_comparison(device int *out [[buffer(0)]],
                                        constant uint *in [[buffer(1)]],
                                        constant int *sIn [[buffer(2)]],
                                        uint index [[thread_position_in_grid]])
{
    int s = -1;
    uint u = in[index];

    // With s = -1 these are the answers the unsigned reading gives, and the
    // signed reading gives the opposite for all of them.
    if (s < u) { out[0] = 1; } else { out[0] = 0; }
    if (s > u) { out[1] = 1; } else { out[1] = 0; }
    if (s <= u) { out[2] = 1; } else { out[2] = 0; }
    if (s >= u) { out[3] = 1; } else { out[3] = 0; }

    // Equality does not depend on the order, and OpIEqual takes both signednesses.
    if (s == u) { out[4] = 1; } else { out[4] = 0; }
    if (s != u) { out[5] = 1; } else { out[5] = 0; }

    // A signed operand loaded rather than declared, so the conversion is reached
    // from a value the compiler did not already know.
    if (sIn[index] < u) { out[6] = 1; } else { out[6] = 0; }
}