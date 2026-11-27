// EXPECT: error dot takes float or half vectors
// OpDot is floating point only, and so is Metal's dot.
kernel void math_dot_of_integer_rejected(
    device const int3* v [[buffer(0)]],
    device int* out [[buffer(1)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = dot(v[i], v[i + 1u]);
}
