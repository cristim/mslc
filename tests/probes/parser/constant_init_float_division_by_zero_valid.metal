// EXPECT: valid
//
// Apple compiles a floating division by zero in a constant brace-list initialiser without a
// diagnostic (#160).
kernel void constant_init_float_division_by_zero_valid(device float *out [[buffer(0)]])
{
    float x{1.0f / 0.0f};
    out[0] = x;
}
