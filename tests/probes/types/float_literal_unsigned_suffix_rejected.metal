// EXPECT: error invalid numeric literal "1.5u"


//
// A floating literal takes no u. It lexed as the integer 1.
constant float kValue = 1.5u;

kernel void float_literal_unsigned_suffix_rejected(device float* out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    out[i] = kValue;
}
