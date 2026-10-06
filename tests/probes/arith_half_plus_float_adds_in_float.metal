// EXPECT: valid
// DISASM-MATCH: = OpFConvert %float %[0-9]+
// DISASM-MATCH: = OpFAdd %float %[0-9]+ %[0-9]+
// DISASM-NOT: = OpFAdd %half
//
// half + float is float: the half widens, the float is not narrowed to half.
kernel void arith_half_plus_float_adds_in_float(device float *out [[buffer(0)]],
                                                device const half *h [[buffer(1)]],
                                                device const float *f [[buffer(2)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = h[i] + f[i];
}
