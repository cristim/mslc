// EXPECT: error on a bool vector is not lowered yet
//
// Apple accepts arithmetic on a bool vector; only the scalar promotion is lowered, and the vector
// form is diagnosed, not miscompiled.
kernel void bool_vector_arithmetic_rejected(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool2 a = bool2(v[i] > 3u, v[i + 1] > 3u);
    out[i] = int2(a + a).x;
}
