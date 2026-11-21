// EXPECT: error an array of bool, which mslc does not lay out yet
struct F { bool flags[4]; };

kernel void array_member_bool_rejected(device uint *out [[buffer(0)]],
    constant F &f [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = f.flags[i & 3] ? 1u : 0u;
}
