// EXPECT: error declares before this one
//
// A one-element brace on a vector is not an integer constant either.
constant int2 N = {3};
kernel void array_length_constant_vector_name_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    out[i] = a[0];
}
