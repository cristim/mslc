// EXPECT: valid
// DISASM: OpTypeInt 32 1
//
// signed alone is int.
kernel void k(device int* o [[buffer(0)]])
{
    signed a = -7;
    o[0] = a;
}
