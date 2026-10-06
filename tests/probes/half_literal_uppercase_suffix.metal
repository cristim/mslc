// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_998pn4
// DISASM-NOT: OpFAdd %float
//
// Apple takes H as it takes h: 0.1H is a half.
kernel void half_literal_uppercase_suffix(device half *out [[buffer(0)]],
                                          device const half *in [[buffer(1)]],
                                          uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + 0.1H;
}
