typedef unsigned short us;
unsigned short widen(unsigned char a, const unsigned int b) { return a + b; }
us twice(us v) { return v * 2; }
kernel void k(device uint* o [[buffer(0)]])
{
    for (unsigned int i = 0; i < 3; i++) {
        o[i] = twice(widen(5, i));
    }
}
