// EXPECT: error narrowing
//
// The bool folds as an int, so true + 300 is 301 and does not fit a char; the narrowing proof still
// rejects it, as Apple does.
kernel void bool_constant_arithmetic_narrowing_rejected(device int *oi [[buffer(0)]])
{
    char x{true + 300};
    oi[0] = x;
}
