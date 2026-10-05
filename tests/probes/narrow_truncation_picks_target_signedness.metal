// EXPECT: valid
// DISASM-MATCH: = OpUConvert %uchar
// DISASM-MATCH: = OpSConvert %short
// DISASM-NO-MATCH: = OpSConvert %uchar
// DISASM-NO-MATCH: = OpUConvert %short
//
// A narrowing conversion across signedness: an %int truncated to uchar and a
// %uint truncated to short. The opcode follows the target's signedness, so these
// are OpUConvert to uchar and OpSConvert to short. Within one signedness the
// source's and the target's choice coincide, which is why narrow_signed_kinds
// and narrow_unsigned_kinds alone cannot tell them apart.
kernel void narrow_truncation_picks_target_signedness(device uint *out [[buffer(0)]],
                                                      constant int *in [[buffer(1)]],
                                                      uint index [[thread_position_in_grid]])
{
    int a = in[index];
    uint u = uint(a);

    uchar c = uchar(a);
    short s = short(u);

    out[0] = uint(c);
    out[1] = uint(s);
}
