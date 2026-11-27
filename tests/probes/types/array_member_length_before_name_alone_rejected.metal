// EXPECT: error puts its length before the name
struct S { float[2] values; };

kernel void array_member_length_before_name_alone_rejected(device float *out [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = s.values[0];
}
