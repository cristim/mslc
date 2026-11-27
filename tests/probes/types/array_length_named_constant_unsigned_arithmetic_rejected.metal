// EXPECT: error subtracts below zero would wrap as an unsigned one
//
// Apple folds "N - 4" as an unsigned subtraction; mslc does not model it, so it says so
// rather than fold it as an int.
constant uint N = 3;
kernel void array_length_named_constant_unsigned_arithmetic_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N - 4 + 10];
    out[i] = a[0];
}
