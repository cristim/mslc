// EXPECT: valid
// DISASM-MATCH: = OpFAdd %half %[0-9]+ %half_0x1_334pn2
// DISASM-MATCH: = OpConstant %float 0\.300048828
//
// 0.3h has a fraction of 204.8 in units of 2^-10, which rounds to 205 (0x1.334p-2,
// the value on the Apple GPU), where truncating gives 204. The float constant is the
// same half widened, folded at file scope.
constant float widened = 0.3h;

kernel void half_literal_normal_rounds_to_nearest(device half *out [[buffer(0)]],
                                                  device float *wide [[buffer(1)]],
                                                  device const half *in [[buffer(2)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = in[i] + 0.3h;
    wide[i] = widened;
}
