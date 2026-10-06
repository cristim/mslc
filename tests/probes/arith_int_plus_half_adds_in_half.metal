// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %half %[0-9]+
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %[0-9]+
//
// An integer beside a half is converted to half, not to float.
kernel void arith_int_plus_half_adds_in_half(device half *out [[buffer(0)]],
                                             device const int *in [[buffer(1)]],
                                             device const half *h [[buffer(2)]],
                                             uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + h[i];
}
