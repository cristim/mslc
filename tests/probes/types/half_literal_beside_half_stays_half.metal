// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_4p_1
// DISASM-NOT: OpFAdd %float
// DISASM-NOT: OpFConvert
//
// An h literal is a half: half + 2.5h is a half sum with the literal as it is.
// The lexer took h and f alike as a float, so this was computed in float.
kernel void half_literal_beside_half_stays_half(device half *out [[buffer(0)]],
                                                device const half *in [[buffer(1)]],
                                                uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + 2.5h;
}
