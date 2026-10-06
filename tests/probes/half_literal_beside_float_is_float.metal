// EXPECT: valid
// DISASM-MATCH: = OpFConvert %float %half_0x1_998pn4
// DISASM-MATCH: = OpFAdd %float %[0-9]+ %[0-9]+
//
// float + 0.1h is a float sum, with the half literal widened exactly.
kernel void half_literal_beside_float_is_float(device float *out [[buffer(0)]],
                                               device const float *in [[buffer(1)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + 0.1h;
}
