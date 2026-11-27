// EXPECT: error the arguments of min have to be one type
// Scalars of different types are not converted; Apple reports the call as
// ambiguous, and converting would truncate min(0, 0.5) to 0.
kernel void math_min_of_int_and_float_rejected(
    device const int* s [[buffer(0)]],
    device const float* f [[buffer(1)]],
    device float* out [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = min(s[i], f[i]);
}
