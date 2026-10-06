// EXPECT: error invalid numeric literal "0x1fh"


//
// The f is a hex digit, and an h after a hex literal makes no literal. It lexed as
// the half 31.
constant half kValue = 0x1fh;

kernel void hex_literal_half_suffix_rejected(device half* out [[buffer(0)]],
                                             uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
