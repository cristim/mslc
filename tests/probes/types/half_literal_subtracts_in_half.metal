// EXPECT: valid
// DISASM-MATCH: = OpFSub %half %[0-9]+ %half_0x1_998pn4
// DISASM-NOT: OpFSub %float
kernel void half_literal_subtracts_in_half(device half *out [[buffer(0)]],
                                           device const half *in [[buffer(1)]],
                                           uint i [[thread_position_in_grid]])
{
    out[i] = in[i] - 0.1h;
}
