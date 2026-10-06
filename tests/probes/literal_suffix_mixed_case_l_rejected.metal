// EXPECT: error invalid suffix "lL" on integer constant
//
// Two l's have to be written alike. Apple rejects lL as well.
kernel void literal_suffix_mixed_case_l_rejected(device long* out [[buffer(0)]],
                                                 uint i [[thread_position_in_grid]])
{
    out[i] = 7lL;
}
