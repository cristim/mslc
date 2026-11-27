// EXPECT: error an array member is used as a whole value
struct W { float a[4]; };

kernel void array_member_whole_value_rejected(device float *out [[buffer(0)]],
    constant W &w [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = w.a;
}
