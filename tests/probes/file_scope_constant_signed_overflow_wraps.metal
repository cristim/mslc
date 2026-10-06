// EXPECT: valid
// DISASM: = OpConstant %int -2147483648
//
// A signed sum past INT_MAX wraps in two's complement, which is what the
// lowering's OpIAdd does and what Apple's compiler folds it to, with a warning.
constant int kValue = 2147483647 + 1;

kernel void file_scope_constant_signed_overflow_wraps(device int* out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
