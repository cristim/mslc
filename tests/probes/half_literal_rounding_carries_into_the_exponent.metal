// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1p_11
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1p_12
// DISASM-MATCH: = OpConstant %float 2048
//
// 2047.9h and 4095.9h round up to a fraction of 1024 units, which is the next power
// of two (0x6800 and 0x6c00 on the Apple GPU), not a fraction bit ORed into an
// exponent that may already have it set.
constant float widened = 2047.9h;

kernel void half_literal_rounding_carries_into_the_exponent(device half *out [[buffer(0)]],
                                                            device float *wide [[buffer(1)]],
                                                            device const half *in [[buffer(2)]],
                                                            uint i [[thread_position_in_grid]])
{
    out[i] = (in[i] + 2047.9h) + 4095.9h;
    wide[i] = widened;
}
