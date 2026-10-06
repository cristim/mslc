// EXPECT: valid
// DISASM: OpTypeInt 64 1
//
// long int is long: Apple sizeof 8, signed.
kernel void k(device long* o [[buffer(0)]])
{
    long int a = -2;
    o[0] = a;
}
