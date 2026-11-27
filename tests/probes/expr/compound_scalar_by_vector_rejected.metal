// EXPECT: error cannot take a vector on the right of a scalar
//
// Apple: cannot convert between vector values of different size.
kernel void compound_scalar_by_vector_rejected(device int *out [[buffer(0)]], device const int4 *v [[buffer(1)]],
                                               uint i [[thread_position_in_grid]])
{
    int x = out[i];
    x += v[i];
    out[i] = x;
}
