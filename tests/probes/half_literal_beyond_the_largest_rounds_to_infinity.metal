// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_ffcp_15
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1p_16
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1p_16_0
//
// 65519.0h is the largest half, 65504. 65520.0h is halfway to the next step and
// ties to even, which is infinity (spirv-dis prints it as 0x1p+16). Apple's
// fast-math compile does not store a result computed from it, so there is no
// device value to compare, only the half that is emitted. A value far above that, 1e6h, is infinity too.
kernel void half_literal_beyond_the_largest_rounds_to_infinity(device half *out [[buffer(0)]],
                                                               device const half *in [[buffer(1)]],
                                                               uint i [[thread_position_in_grid]])
{
    out[i] = ((in[i] + 65519.0h) + 65520.0h) + 1e6h;
}
