// EXPECT: error not a constant
//
// A local variable hides the constant: the length names a variable (Apple: "array size is
// not a constant expression").
constant int N = 3;
kernel void array_length_named_constant_shadowed_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    int N = 2;
    float a[N];
    a[0] = 1.0;
    out[i] = a[0];
}
