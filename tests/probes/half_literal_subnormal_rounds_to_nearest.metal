// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_5pn17
// DISASM-MATCH: = OpConstant %float 1\.00135803e-05
//
// 1e-5h is a half subnormal: 167.77 units of 2^-24 rounds to 168 (0x00a8 on the
// Apple GPU), where truncating gives 167.
constant float widened = 1e-5h;

kernel void half_literal_subnormal_rounds_to_nearest(device half *out [[buffer(0)]],
                                                     device float *wide [[buffer(1)]],
                                                     device const half *in [[buffer(2)]],
                                                     uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + 1e-5h;
    wide[i] = widened;
}
