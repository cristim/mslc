// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float %[0-9]+
// DISASM-MATCH: = OpFAdd %float %[0-9]+ %float_1_5
// DISASM-NOT: OpConvertFToS
// DISASM-NOT: OpIAdd
//
// int + float is a float sum: the int is converted up and the 1.5 is kept. The
// emitter converted the right operand to the left's type instead, so this added 1.
kernel void arith_int_plus_float_adds_in_float(device float *out [[buffer(0)]],
                                               device const int *in [[buffer(1)]],
                                               uint i [[thread_position_in_grid]])
{
    int x = in[i];
    out[i] = x + 1.5f;
}
