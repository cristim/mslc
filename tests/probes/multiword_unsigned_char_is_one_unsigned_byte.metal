// EXPECT: valid
// DISASM: OpTypeInt 8 0
// DISASM-NOT: OpTypeInt 8 1
//
// unsigned char is uchar: 1 byte, unsigned (Apple sizeof 1, (T)-1 > 0).
struct S { unsigned char a; float b; };
kernel void k(device S* o [[buffer(0)]])
{
    o[0].a = 200;
}
