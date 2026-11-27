// EXPECT: valid
//
// Apple compiles a negative shift count in a constant brace-list initialiser with a warning (#160).
kernel void constant_init_invalid_shift_valid(device int *out [[buffer(0)]])
{
    int x{1 << -1};
    out[0] = x;
}
