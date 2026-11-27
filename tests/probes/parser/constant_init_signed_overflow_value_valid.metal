// EXPECT: valid
// DISASM: OpConstant %int -2147483648
// DISASM: OpIAdd %int %int_2147483647 %int_1
//
// Apple folds 2147483647 + 1 to -2147483648 with a warning, at file scope and in a brace-list
// initialiser. mslc folds the file-scope constant to the same value and leaves the brace-list
// add to the backend, whose int arithmetic wraps, so both contexts answer #160's question:
// the produced values are the same as Apple's. The folded constant is the durable value pin;
// the diffcheck run at seed 1 against Apple covers the runtime one.
constant int k = 2147483647 + 1;

kernel void constant_init_signed_overflow_value_valid(device int *out [[buffer(0)]])
{
    int x{2147483647 + 1};
    out[0] = x;
    out[1] = k;
    out[2] = (x == k) ? 1 : 0;
}
