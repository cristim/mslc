// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %half %[0-9]+
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_4p_1
// DISASM-NOT: OpFAdd %float
//
// int + 2.5h is a half sum, as it is in Apple's compiler: 2049 + 2.5h is 2050.
kernel void half_literal_beside_int_is_half(device half *out [[buffer(0)]],
                                            device const int *in [[buffer(1)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + 2.5h;
}
