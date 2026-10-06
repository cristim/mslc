// EXPECT: valid
// DISASM-MATCH: = OpConstant %half 0x1\.998p-4
// DISASM-NOT: OpConvert
kernel void half_literal_stored_as_half(device half *out [[buffer(0)]],
                                        uint i [[thread_position_in_grid]])
{
    half x = 0.1h;
    out[i] = x;
}
