// EXPECT: error a vector is not implicitly converted to a vector of another component type
//
// Apple: "cannot initialize a variable of type 'float2' with an lvalue of type
// 'bool2'". The constructor form is the lowered one.
kernel void bool_vector_implicit_to_float_vector_rejected(device float2 *out [[buffer(0)]], constant uint2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool2 b = bool2(v[i].x > 1u, v[i].y > 1u);
    float2 r = b;
    out[i] = r;
}
