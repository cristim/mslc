// EXPECT: valid
// DISASM-MATCH: = OpFMul %half %[0-9]+ %half_0x1_998pn4
// DISASM-NOT: %float_0_1
//
// 0.1h is the half nearest 0.1, which is 0.0999756, not the float 0.1 converted.
kernel void half_literal_is_rounded_to_half(device half *out [[buffer(0)]],
                                            device const half *in [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = in[i] * 0.1h;
}
