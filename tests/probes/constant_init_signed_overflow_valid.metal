// EXPECT: valid
//
// Apple compiles a signed overflow in a constant brace-list initialiser with a warning and wraps
// the value, so mslc accepts it too (#160).
kernel void constant_init_signed_overflow_valid(device int *out [[buffer(0)]])
{
    int x{2147483647 + 1};
    out[0] = x;
}
