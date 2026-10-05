// EXPECT: valid
// DISASM: OpCapability Int8
// DISASM: OpCapability Int16
// DISASM: OpCapability Int64
// DISASM: OpCapability Float16
// DISASM-MATCH: = OpConvertSToF %float
// DISASM-MATCH: = OpSConvert %
// DISASM-MATCH: = OpFConvert %
// DISASM-NO-MATCH: = OpConvertUToF
// DISASM-NO-MATCH: = OpUConvert
//
// Char, short and long converted from a %int and back out to float. Every
// integer in this module is signed, so the conversions must be OpSConvert and
// OpConvertSToF and an unsigned opcode anywhere fails the NO-MATCH. The
// unsigned kinds are in narrow_unsigned_kinds, a separate module, because
// DISASM matches the whole module: with both in one file, swapping the two
// signedness choices would still satisfy every needle.
//
// `double` is not here because xcrun metal rejects it ("'double' is not
// supported in Metal"); double_scalar covers its capability.
kernel void narrow_signed_kinds(device float *out [[buffer(0)]],
                                constant int *in [[buffer(1)]],
                                uint index [[thread_position_in_grid]])
{
    int a = in[index];

    char c = char(a);   out[0] = float(c);
    short s = short(a); out[1] = float(s);
    long l = long(a);   out[2] = float(l);

    half h = half(a);   out[3] = float(h);
}
