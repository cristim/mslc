// EXPECT: error is a variable, not a constant
//
// A local reference is a variable here, even to a constexpr (Apple: not a constant expression).
kernel void array_length_constant_reference_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    constexpr int base = 3;
    thread const int &N = base;
    float a[N];
    out[i] = a[0];
}
