// EXPECT: error is outside the array member of 4
struct R { float a[4]; };

kernel void array_member_index_out_of_range_rejected(device float *out [[buffer(0)]],
    constant R &r [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = r.a[4];
}
