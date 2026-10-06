// EXPECT: valid
// DISASM: OpTypeInt 16 0
//
// device const unsigned short* and constant unsigned short*.
kernel void k(device uint* o [[buffer(0)]], device const unsigned short* p [[buffer(1)]], constant unsigned short* q [[buffer(2)]])
{
    o[0] = p[0] + q[0];
}
