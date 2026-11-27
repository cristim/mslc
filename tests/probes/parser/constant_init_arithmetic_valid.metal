// EXPECT: valid
// A valid constant initialiser of the same type still compiles (#160).
kernel void constant_init_arithmetic_valid(device float *out [[buffer(0)]])
{
    float x{1 + 2};
    out[0] = x;
}
