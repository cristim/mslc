// EXPECT: valid
// DISASM: OpSLessThan
// DISASM: OpSGreaterThan
// DISASM: OpSLessThanEqual
// DISASM: OpSGreaterThanEqual
// DISASM-NOT: OpBitcast %ushort
// DISASM-NOT: OpBitcast %uint
// DISASM-NOT: OpBitcast %short
//
// The integer promotions, which come before the usual arithmetic conversions and
// were missing entirely.
//
// Anything narrower than int becomes int whatever its signedness, so a negative
// char beside a ushort compares -1 against 0 as two ints, which is true. mslc
// went straight to the comparison and picked the wider type instead, so -1
// widened to 65535 and the answer came back false.
//
// The three NOT pins are the shape the wrong answer had. Promoting a char to a
// ushort is a sign extension, which SPIR-V spells as an OpBitcast to the unsigned
// type rather than an OpSConvert, and the comparison then reads 65535 against 0.
// There is no such instruction in the module, so all four comparisons are between
// two ints that a sign extension reaches without changing the value.
//
// Only the narrower-than-int types are here. A uint beside an int is already the
// wider type and the tie-break handles it, so promoting would be wrong: an int
// beside a uint is an unsigned comparison, which is the rule rather than a bug.
//
// The float case is absent on purpose. A half beside a float is a different
// conversion, OpFConvert, and folding it in here would make the pins about
// promotion say nothing.
kernel void integer_promotions_in_comparisons(device float *out [[buffer(0)]],
                                              uint index [[thread_position_in_grid]])
{
    char c = -1;
    uchar uc = 255;
    short s = -1;
    ushort us = 0;

    if (c < us) { out[0] = 1.0; } else { out[0] = 0.0; }
    if (c > uc) { out[1] = 1.0; } else { out[1] = 0.0; }
    if (s <= uc) { out[2] = 1.0; } else { out[2] = 0.0; }
    if (uc >= s) { out[3] = 1.0; } else { out[3] = 0.0; }
}