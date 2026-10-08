kernel void k(device uint* o [[buffer(0)]], device float* f [[buffer(1)]])
{
    float x = f[0];
    o[0] = (uint)x + (uint)x + (uchar)x + (ushort)x + (ulong)x + (uint)x + (uint)x + (uint)x + uint(x);
    o[1] = (char)x + (int)x + (short)x + (long)x + (ulong)x + int(x);
}
