// EXPECT: valid
// DISASM: OpTypeInt 8 0
// DISASM: OpTypeInt 16 0
//
// A helper takes and returns multi-word types.
unsigned short widen(unsigned char a)
{
    return a;
}
kernel void k(device uint* o [[buffer(0)]])
{
    o[0] = widen(5);
}
