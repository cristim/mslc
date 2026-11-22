// EXPECT: error requires a supported constant expression
//
// Signed overflow in a constant initialiser is not a constant expression, so the brace list is refused
// even though int to int is no narrowing (#160).
kernel void constant_init_signed_overflow_rejected(device int *out [[buffer(0)]])
{
    int x{2147483647 + 1};
    out[0] = x;
}
