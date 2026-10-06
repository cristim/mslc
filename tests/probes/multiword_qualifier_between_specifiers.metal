// EXPECT: valid
// DISASM: OpTypeInt 32 0
//
// const may sit between the specifiers: unsigned const int.
kernel void k(device uint* o [[buffer(0)]])
{
    unsigned const int a = 4;
    o[0] = a;
}
