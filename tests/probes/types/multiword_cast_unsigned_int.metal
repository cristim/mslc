// EXPECT: valid
// DISASM: OpConvertFToU %uint
//
// (unsigned int)x is the cast to uint.
kernel void k(device uint* o [[buffer(0)]])
{
    float x = 3.5;
    o[0] = (unsigned int)x;
}
