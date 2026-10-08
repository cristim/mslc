// EXPECT: valid
// DISASM: OpConvertFToU %uint
//
// unsigned(x) is valid: unsigned is one word. Apple accepts it.
kernel void k(device uint* o [[buffer(0)]])
{
    float x = 3.5;
    o[0] = unsigned(x);
}
