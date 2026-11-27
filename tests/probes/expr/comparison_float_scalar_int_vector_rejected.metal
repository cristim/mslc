// EXPECT: error a floating-point scalar cannot be combined with an integer vector
//
kernel void comparison_float_scalar_int_vector_rejected(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant float *s [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(s[i] < a[i]);
}
