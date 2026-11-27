// EXPECT: valid
// DISASM-MATCH: = OpFConvert %float %[0-9]+
// DISASM-MATCH: = OpFAdd %float %[0-9]+ %float_1
// DISASM-MATCH: = OpFConvert %half %[0-9]+
//
// half h; h += 1.0f adds in float, because a float beside a half makes both float, and converts
// the sum back to half.
kernel void compound_half_by_float_adds_in_float(device half *out [[buffer(0)]],
                                                 uint i [[thread_position_in_grid]])
{
    half x = out[i];
    x += 1.0f;
    out[i] = x;
}
