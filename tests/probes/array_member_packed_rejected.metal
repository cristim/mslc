// EXPECT: error an array of a packed vector, which mslc does not lay out yet
struct P { packed_float3 v[4]; };

kernel void array_member_packed_rejected(device float *out [[buffer(0)]],
    constant P &p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = p.v[i & 3].x;
}
