// EXPECT: valid
// DISASM: OpTypeInt 8 1
// DISASM-NOT: OpTypeInt 8 0
//
// signed char is char: 1 byte, signed.
struct S { signed char a; float b; };
kernel void k(device S* o [[buffer(0)]])
{
    o[0].a = -3;
}
