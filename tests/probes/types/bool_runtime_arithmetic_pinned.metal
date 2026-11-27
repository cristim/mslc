// EXPECT: valid
// DISASM: OpBitwiseAnd %int
// DISASM: OpLogicalNot %bool
// DISASM: OpBitwiseXor %int
// DISASM: OpLogicalEqual %bool
// DISASM-NOT: OpLogicalAdd
//
// Issue #160: runtime bool operators. a + b promotes to int; !a & b, a ^ b and a == b are accepted
// by Apple (checked with -S -emit-llvm).
kernel void bool_runtime_arithmetic_pinned(device int *o [[buffer(0)]])
{
    bool a = o[0] != 0;
    bool b = o[1] != 0;
    int s = a + b;
    bool c = !a & b;
    bool x = a ^ b;
    bool e = a == b;
    o[2] = s + c + x + e;
}
