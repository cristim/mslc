kernel void k(device uint* o [[buffer(0)]], device const uint* in [[buffer(1)]])
{
    unsigned unsigned short a = 65535;
    o[0] = uint(a) + in[0];
    o[1] = uint(a + 1) + in[0];
}
