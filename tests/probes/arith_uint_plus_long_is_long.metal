// EXPECT: valid
// DISASM-MATCH: = OpUConvert %ulong %[0-9]+|= OpSConvert %long %[0-9]+
// DISASM-MATCH: = OpIAdd %long %[0-9]+ %[0-9]+
// DISASM-NOT: = OpIAdd %uint
//
// uint + long is a long sum: the wider type wins, so the uint is not the
// common type and the long is not narrowed to 32 bits.
kernel void arith_uint_plus_long_is_long(device long *out [[buffer(0)]],
                                         device const uint *a [[buffer(1)]],
                                         device const long *b [[buffer(2)]],
                                         uint i [[thread_position_in_grid]])
{
    out[i] = a[i] + b[i];
}
