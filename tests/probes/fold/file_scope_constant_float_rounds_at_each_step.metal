// EXPECT: valid
// DISASM: = OpConstant %float 16777216
// DISASM-NOT: = OpConstant %float 16777218
//
// Each float operation rounds to a float: 2^24 + 1 is 2^24 again, so adding 1
// twice leaves 16777216. Folded in a double and rounded once at the end it was
// 16777218.
constant float kValue = 16777216.0f + 1.0f + 1.0f;

kernel void file_scope_constant_float_rounds_at_each_step(device float* out [[buffer(0)]],
                                                          uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
