// EXPECT: valid
// DISASM: OpConvertFToU %uint
//
// A qualifier in a cast to a scalar changes nothing; Apple accepts it.
kernel void k(device uint* o [[buffer(0)]])
{
    float x = 3.5;
    o[0] = (const unsigned int)x;
}
