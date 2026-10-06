// EXPECT: valid
// DISASM-MATCH: = OpUDiv %uint %[0-9]+ %[0-9]+
// DISASM-NOT: OpSDiv
//
// int / uint is an unsigned division: C converts the int to uint. The emitter
// converted the uint to int and divided signed, so -8 / 2u was -4, not 2147483644.
kernel void arith_int_divided_by_uint_is_unsigned(device uint *out [[buffer(0)]],
                                                  device const int *a [[buffer(1)]],
                                                  device const uint *b [[buffer(2)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = a[i] / b[i];
}
