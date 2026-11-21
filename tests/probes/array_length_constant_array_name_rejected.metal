// EXPECT: error declares before this one
//
// A one-element constant array is not an integer, though its brace holds one (Apple: not
// implicitly convertible to 'unsigned long').
constant int N[1] = {3};
kernel void array_length_constant_array_name_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
