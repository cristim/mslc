// EXPECT: error needs a constant integer argument
//
// short(70000) is 4464 in Apple; the constructor is not an int, so it is not unwrapped to 70000.
constexpr constant int N = short(70000);
kernel void array_length_constant_narrowing_cast_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
