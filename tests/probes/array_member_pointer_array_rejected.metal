// EXPECT: error is an array of pointers
struct S { device float *p[2]; };

kernel void array_member_pointer_array_rejected(device float *out [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = s.p[0][0];
}
