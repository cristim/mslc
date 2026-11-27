// EXPECT: error needs a length
struct U { float a[]; };

kernel void array_member_unsized_rejected(device float *out [[buffer(0)]],
    constant U &u [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = u.a[0];
}
