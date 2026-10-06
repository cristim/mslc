// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float %[0-9]+
// DISASM-MATCH: = OpFSub %float %float_0_5 %[0-9]+
// DISASM-NOT: OpISub
//
// A float on the left and an int on the right: the sum is still float.
kernel void arith_float_minus_int_subtracts_in_float(device float *out [[buffer(0)]],
                                                     device const int *in [[buffer(1)]],
                                                     uint i [[thread_position_in_grid]])
{
    int x = in[i];
    out[i] = 0.5f - x;
}
