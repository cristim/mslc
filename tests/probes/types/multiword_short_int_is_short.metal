// EXPECT: valid
// DISASM: OpTypeInt 16 1
// DISASM-NOT: OpTypeInt 16 0
//
// short int is short: 2 bytes, signed.
kernel void k(device short* o [[buffer(0)]])
{
    short int a = -2;
    o[0] = a;
}
