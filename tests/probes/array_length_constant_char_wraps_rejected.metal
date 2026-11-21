// EXPECT: error declares before this one
//
// "constant char C = 200" wraps to -56 in Apple (a[C + 60] has 4 elements); refused, not 260.
constant char C = 200;
kernel void array_length_constant_char_wraps_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[C + 60];
    out[i] = a[0];
}
