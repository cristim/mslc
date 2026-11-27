// EXPECT: valid
// DISASM-MATCH: = OpConvertSToF %float %[0-9]+
// DISASM-MATCH: = OpFAdd %float %[0-9]+ %float_1_5
// DISASM-MATCH: = OpConvertFToS %int %[0-9]+
//
// int x; x += 1.5f adds in float and converts the sum back to int, as x = (int)(x + 1.5f).
kernel void compound_int_by_float_converts_back(device int *out [[buffer(0)]],
                                                uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x += 1.5f;
    out[i] = x;
}
