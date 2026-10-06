// EXPECT: error a floating-point scalar cannot be combined with an integer vector
//
// Apple: "cannot convert between vector values of different size".
kernel void ternary_float_scalar_with_int_vector_rejected(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int3 w = int3(i);
    float s = in[0];
    int3 r = in[1] > 0.0f ? w : s;
    out[i] = float(r.x);
}
