// EXPECT: valid
// DISASM: OpTypeInt 16 0
// DISASM-NOT: OpTypeInt 16 1
//
// unsigned short is ushort, as iTermShaderTypes.h:119 spells runLength.
struct S { unsigned short runLength; float b; };
kernel void k(device S* o [[buffer(0)]])
{
    o[0].runLength = 9;
}
