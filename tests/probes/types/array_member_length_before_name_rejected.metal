// EXPECT: error puts its length before the name
// "float[2] a[3]" used to drop the [2] and lay the member out as float[3].
struct S { float[2] a[3]; float b; };

kernel void array_member_length_before_name_rejected(device float *out [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = s.b;
}
