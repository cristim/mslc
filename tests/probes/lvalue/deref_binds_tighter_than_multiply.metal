// EXPECT: valid
// DISASM-MATCH: OpFMul %float %[0-9]+ %float_2
//
// "*p * 2.0" is (*p) * 2.0, with the unary '*' on the left and the binary one
// between the operands.
kernel void deref_binds_tighter_than_multiply(device float* out [[buffer(0)]],
                                              device const float* in [[buffer(1)]])
{
    out[0] = *in * 2.0;
}
