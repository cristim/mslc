// EXPECT: valid
// DISASM: OpCapability Int8
// DISASM: OpCapability Int16
// DISASM: OpCapability Int64
// DISASM-MATCH: = OpConvertUToF %float
// DISASM-MATCH: = OpUConvert %
// DISASM-NO-MATCH: = OpConvertSToF
// DISASM-NO-MATCH: = OpSConvert
//
// The unsigned counterpart of narrow_signed_kinds: uchar, ushort and ulong
// converted from a %uint and back out to float. Every integer here is unsigned,
// so a signed convert opcode anywhere fails the NO-MATCH.
kernel void narrow_unsigned_kinds(device float *out [[buffer(0)]],
                                  constant uint *in [[buffer(1)]],
                                  uint index [[thread_position_in_grid]])
{
    uint a = in[index];

    uchar c = uchar(a);   out[0] = float(c);
    ushort s = ushort(a); out[1] = float(s);
    ulong l = ulong(a);   out[2] = float(l);
}
