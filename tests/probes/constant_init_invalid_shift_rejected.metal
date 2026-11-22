// EXPECT: error requires a supported constant expression
//
// A shift by a negative count is not a constant expression (#160).
kernel void constant_init_invalid_shift_rejected(device int *out [[buffer(0)]])
{
    int x{1 << -1};
    out[0] = x;
}
