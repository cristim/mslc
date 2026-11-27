// EXPECT: valid
// DISASM-MATCH: OpFMul %float %float_2 %[0-9]+
//
// "2.0 * *p": the first '*' is binary and the second is unary.
kernel void deref_as_the_right_multiplicand(device float* out [[buffer(0)]],
                                            device const float* in [[buffer(1)]])
{
    out[0] = 2.0 * *in;
}
