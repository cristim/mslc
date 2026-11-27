// EXPECT: valid
// DISASM-MATCH: = OpIMul %int %[0-9]+ %[0-9]+
// DISASM-NOT: = OpIMul %uchar
//
// 200 * 200 as uchar operands is 40000, not 64: both are promoted to int first.
kernel void arith_uchar_times_uchar_promotes_to_int(device int *out [[buffer(0)]],
                                                    device const uchar *a [[buffer(1)]],
                                                    device const uchar *b [[buffer(2)]],
                                                    uint i [[thread_position_in_grid]])
{
    out[i] = a[i] * b[i];
}
