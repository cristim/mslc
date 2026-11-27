// EXPECT: valid
// DISASM-MATCH: = OpSLessThan %bool
// DISASM-NOT: OpBitcast %ulong
//
// Rank decides, not signedness, when the two widths differ.
//
// C's usual arithmetic conversions promote first, then take the higher-rank type
// outright, signedness and all. A long has the higher rank against a uint and can
// represent every uint value, so the comparison stays signed. Signedness only
// decides when the two ranks are equal, which is the one case it decides at all.
//
// mslc broke the tie on signedness whenever the widths differed, so this pair came
// out as OpBitcast %ulong and OpULessThan: 4294967295 against a small value, which
// answers false where MSL says true. Both build here and on master, and the module
// validates either way, so the pin is the signed opcode.
//
// This is a probe of its own for a harness reason rather than a semantic one.
// DISASM is whole-module containment with no attribution to a comparison, so an
// OpULessThan from this pair would satisfy the OpULessThan pin in
// integer_promotions_in_comparisons even if every narrow-versus-uint case there
// were signed. One pair per probe is what keeps that pin honest.
//
// DISASM-MATCH rather than DISASM because "OpSLessThan" is a prefix of
// "OpSLessThanEqual", so containment would be satisfied by the wrong operator.
kernel void wider_rank_beats_signedness_in_a_comparison(device int *out [[buffer(0)]],
                                                        uint index [[thread_position_in_grid]])
{
    long l = -1;
    uint u = 0u;

    if (l < u) { out[0] = 1; } else { out[0] = 0; }
    if (u < l) { out[1] = 1; } else { out[1] = 0; }
}
