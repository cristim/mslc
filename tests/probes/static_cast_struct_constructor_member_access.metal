// EXPECT: error ".x" swizzles a value that is not a vector
// Functional S(v).x has the same existing helper-result member-access limit.
struct S { float x; S(float v) : x(v + 5.0f) {} };
kernel void static_cast_struct_constructor_member_access(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<S>(in[i]).x;
}
