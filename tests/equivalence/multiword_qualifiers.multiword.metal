kernel void k(device uint* o [[buffer(0)]], device const unsigned int* p [[buffer(1)]], device unsigned const* q [[buffer(2)]], constant unsigned short* r [[buffer(3)]])
{
    const unsigned int a = p[0];
    unsigned const int b = q[0];
    unsigned int const c = 3;
    int const unsigned d = 4;
    o[0] = a + b + c + d + r[0];
}
