// EXPECT: valid
// DISASM-MATCH: = OpSConvert %int %[0-9]+
// DISASM-MATCH: = OpIAdd %int %[0-9]+ %int_1
// DISASM-MATCH: = OpSConvert %short %[0-9]+
//
// short s; s += 1 adds as int and truncates back to short: the operands are promoted first.
kernel void compound_narrow_int_promotes(device short *out [[buffer(0)]],
                                         uint i [[thread_position_in_grid]])
{
    short x = out[i];
    x += 1;
    out[i] = x;
}
