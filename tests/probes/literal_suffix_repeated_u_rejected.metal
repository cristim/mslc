// EXPECT: error invalid suffix "uu" on integer constant
//
// One u only.
kernel void literal_suffix_repeated_u_rejected(device uint* out [[buffer(0)]],
                                               uint i [[thread_position_in_grid]])
{
    out[i] = 7uu;
}
