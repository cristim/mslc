typedef ushort us;
ushort widen(uchar a, const uint b) { return a + b; }
us twice(us v) { return v * 2; }
kernel void k(device uint* o [[buffer(0)]])
{
    for (uint i = 0; i < 3; i++) {
        o[i] = twice(widen(5, i));
    }
}
