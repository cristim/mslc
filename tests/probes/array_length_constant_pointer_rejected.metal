// EXPECT: error is a variable, not a constant
//
// A pointer is not an integer constant, whatever it is initialised with (Apple: not a constant
// expression).
kernel void array_length_constant_pointer_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    thread const int *N = 0;
    float a[N];
    out[i] = a[0];
}
