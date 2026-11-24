// EXPECT: valid
// DISASM: OpSNegate %int
// DISASM-NOT: OpLogicalAdd
//
// Issue #160: arithmetic on bool operands in a brace-list initialiser. Apple promotes each bool to
// int, so true + true is 2 and fits int and float; the result only narrows when it targets bool.
kernel void bool_brace_list_arithmetic_valid(device int *o [[buffer(0)]])
{
    int a{true + true};
    int b{true + 1};
    float c{true * 3};
    int d{-true};
    int e{!true + 1};
    constexpr int n{true + true};
    o[0] = a + b + int(c) + d + e + n;
}
