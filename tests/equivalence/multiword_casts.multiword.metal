kernel void k(device uint* o [[buffer(0)]], device float* f [[buffer(1)]])
{
    float x = f[0];
    o[0] = (unsigned)x + (unsigned int)x + (unsigned char)x + (unsigned short)x + (unsigned long)x + (const unsigned int)x + (unsigned const int)x + (unsigned int const)x + unsigned(x);
    o[1] = (signed char)x + (signed)x + (short int)x + (long int)x + (long unsigned int)x + signed(x);
}
