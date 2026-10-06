kernel void k(device uint* o [[buffer(0)]], device const uint* p [[buffer(1)]], device const uint* q [[buffer(2)]], constant ushort* r [[buffer(3)]])
{
    const uint a = p[0];
    const uint b = q[0];
    const uint c = 3;
    const uint d = 4;
    o[0] = a + b + c + d + r[0];
}
