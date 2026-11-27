// EXPECT: valid
// DISASM: OpTypeInt 64 0
//
// unsigned long int is ulong: Apple sizeof 8, unsigned.
kernel void k(device ulong* o [[buffer(0)]])
{
    unsigned long int a = 2;
    o[0] = a;
}
