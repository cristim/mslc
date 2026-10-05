// EXPECT: error cannot take a floating-point scalar on the right of an integer vector
//
// Apple: cannot convert between vector values of different size ('int4' and 'float').
kernel void compound_int_vector_by_float_scalar_rejected(device int4 *out [[buffer(0)]],
                                                         uint i [[thread_position_in_grid]])
{
    int4 x = out[i];
    x += 1.5f;
    out[i] = x;
}
