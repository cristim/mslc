// EXPECT: error declares before this one
//
// "constant uchar C = 300" wraps to 44 in Apple (a[C - 40] has 4 elements). mslc does not model
// the wrap, so the value is not a constant it knows and the length is refused, not taken as 260.
constant uchar C = 300;
kernel void array_length_constant_uchar_wraps_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[C - 40];
    out[i] = a[0];
}
