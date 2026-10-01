// EXPECT: valid
// DISASM: OpCapability Int8
// DISASM: OpCapability Int16
// DISASM: OpCapability Int64
// DISASM: OpCapability Float16
// DISASM: OpConvertSToF
// DISASM: OpConvertUToF
// DISASM: OpSConvert
// DISASM: OpUConvert
// DISASM: OpFConvert
// DISASM-NOT: OpConvertUToS
// DISASM-NOT: OpConvertSToU
//
// Every narrow scalar kind in both signednesses, converted from a %int and
// converted back. The five existing scalar probes each declare one buffer of one
// unsigned kind, which reaches the capability but not the conversion: the
// signedness mapping in sema.cpp had Char, Short, Long, ULong, Half and Double
// unexecuted, and the reverse integerKind() that a width-changing conversion
// reads had 8 and 16 bits and its default unexecuted.
//
// The two NOT pins are the trap. A signed integer widened or narrowed goes
// through OpSConvert and an unsigned one through OpUConvert, and mslc emits
// OpConvertUToF for a %uint to float and OpConvertSToF for a %int, which is the
// distinction a signedness bug gets wrong. OpConvertUToS and OpConvertSToU are
// not in the module at all: neither is a type SPIR-V has, so an emitter that
// reached for one would produce a module spirv-val rejects rather than a wrong
// one, and the pin says the signedness went through the convert opcodes that do
// exist instead.
//
// `double` is in the mapping and mslc can lower it, but xcrun metal rejects it
// ("'double' is not supported in Metal"), so it is not here: a probe whose source
// the reference compiler refuses is asserting behaviour nothing can reach. The
// capability is still covered by double_scalar, which does not convert.
//
// The narrowing cases with a value that does not fit (300 into a char, 70000
// into a short) are here because integerKind() is asked for the target width and
// has to find it, and because a narrowing conversion that wrapped silently would
// be the same class of defect as a widening one that did.
kernel void narrow_scalar_kinds(device float *out [[buffer(0)]],
                                constant int *in [[buffer(1)]],
                                uint index [[thread_position_in_grid]])
{
    int a = in[index];

    // Signed and unsigned, 8 and 16 and 64 bits, converted out to float.
    char c = char(a);      out[0] = float(c);
    uchar uc = uchar(a);   out[1] = float(uc);
    short s = short(a);    out[2] = float(s);
    ushort us = ushort(a); out[3] = float(us);
    long l = long(a);      out[4] = float(l);
    ulong ul = ulong(a);   out[5] = float(ul);

    // 16-bit float, which is a different capability from a 16-bit integer.
    half h = half(a);      out[6] = float(h);
    half fromLiteral = half(1.5); out[7] = float(fromLiteral);

    // Narrowing from a value that does not fit, so the target width has to be
    // resolved rather than inherited.
    char narrow8 = char(300);    out[8] = float(narrow8);
    short narrow16 = short(70000); out[9] = float(narrow16);
    long wide64 = long(5);       out[10] = float(wide64);
}
