// EXPECT: error a condition has to be a bool or a numeric scalar
//
// bool2 || bool.
kernel void logical_or_bool_vector_with_scalar_rejected(device uint2 *out [[buffer(0)]], constant uint2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool2 a = bool2(v[i].x > 1u, v[i].y > 1u);
    bool b = v[i].x > 2u;
    bool2 r = a || b;
    out[i] = uint2(0u);
}
