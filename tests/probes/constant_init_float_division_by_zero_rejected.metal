// EXPECT: error requires a supported constant expression
//
// A floating division by zero is not a constant expression in a brace list (#160).
kernel void constant_init_float_division_by_zero_rejected(device float *out [[buffer(0)]])
{
    float x{1.0f / 0.0f};
    out[0] = x;
}
