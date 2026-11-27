// EXPECT: valid
// DISASM: OpSConvert %int
// DISASM-MATCH: = OpSLessThan %bool
// DISASM-MATCH: = OpULessThan %bool
// DISASM-NOT: OpBitcast %uchar
// DISASM-NOT: OpBitcast %ushort
// DISASM-NOT: OpBitcast %short
//
// The integer promotions, which come before the usual arithmetic conversions and
// were missing entirely. Anything narrower than int becomes int whatever its
// signedness, so a negative char beside a ushort compares -1 against 0 as two
// ints, which is true.
//
// There are two wrong answers here and the pins are aimed at both, because a
// probe that only catches one of them is decoration.
//
// **Widening to the unsigned type.** Going straight to the conversions widened a
// narrow operand to the *other* side's type, so -1 became 65535 against a ushort.
// The sign extension is an OpBitcast to that unsigned narrow type, so forbidding
// OpBitcast %uchar, %ushort and %short is what catches this.
//
// **Promoting both sides to one int.** Making both operands int at once *replaces*
// the conversions rather than preceding them, and then "char c = -1; uint u = 0;
// c < u" compares two ints, is signed, and answers true where MSL leaves the uint
// alone and answers false. That turns the OpULessThan below into an OpSLessThan,
// so pinning it present is what catches this. Promoting one side and then
// applying the conversions gives OpULessTan by the right route, visible as an
// OpSConvert %int followed by an OpBitcast %uint, which is a bitcast to the *32
// bit* unsigned type and not to a narrow one.
//
// A uint beside a narrow type is here rather than only a ushort, because it is
// the pair that tells the two wrong answers apart: widening and promote-both
// disagree about it, and only the correct route leaves the uint alone until the
// conversions make both sides unsigned.
//
// A long beside a uint is here for the rank rule: the wider type wins outright, so
// that comparison stays signed. The tie-break used to fire on signedness whenever
// the widths differed, which turned this pair unsigned.
//
// DISASM is whole-module containment, with no attribution to a comparison, so
// this kernel deliberately contains no long-versus-uint case: an OpULessThan from
// that pair would satisfy the pin below even if every narrow-versus-uint case
// were signed. That pair has its own probe for the same reason.
//
// Containment also means "OpSLessThan" would be satisfied by "OpSLessThanEqual",
// so the pins that care about the exact operator are DISASM-MATCH, which is a
// regex and can include the result type. What neither form can say is which
// conversion feeds which comparison, so the pairing is verified by the 384-pair
// type sweep in the scratch directory rather than here.
kernel void integer_promotions_in_comparisons(device int *out [[buffer(0)]],
                                              constant char *in [[buffer(1)]],
                                              uint index [[thread_position_in_grid]])
{
    char c = -1;
    uchar uc = 255;
    short s = -1;
    ushort us = 0;
    uint u = 0u;

    // Narrow against narrow: promotion, signed, -1 < 0.
    if (c < us) { out[0] = 1; } else { out[0] = 0; }

    // Narrow against unsigned 32-bit: the narrow side is promoted to int and the
    // uint is left alone, so the conversions then make both sides unsigned.
    if (c < u) { out[1] = 1; } else { out[1] = 0; }
    if (uc > u) { out[2] = 1; } else { out[2] = 0; }

    if (s <= uc) { out[3] = 1; } else { out[3] = 0; }
}
