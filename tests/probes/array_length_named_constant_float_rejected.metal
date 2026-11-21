// EXPECT: error not an integer constant
//
// A float constant is not an integer length (Apple: "conversion from 'float' to
// 'unsigned long' is not allowed in a converted constant expression").
constexpr constant float N = 3.0;
kernel void array_length_named_constant_float_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
