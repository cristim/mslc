// EXPECT: valid
// static_cast<S>(v).x reads a member of the temporary the constructor helper returns
// (#158: it used to stop at the ".x swizzles a value that is not a vector" limit).
struct S { float x; S(float v) : x(v + 5.0f) {} };
kernel void static_cast_struct_constructor_member_read(device float* out [[buffer(0)]], constant float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = static_cast<S>(in[i]).x;
}
