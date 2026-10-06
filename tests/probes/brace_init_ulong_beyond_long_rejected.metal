// EXPECT: error which cannot be narrowed to type 'long'
//
// 0x8000000000000000 is a ulong, and 2^63 does not fit a long. Both are 64 bits, so
// reducing it to the long's width changes nothing the check could see: the sign
// is what decides.
constant long2 kValue = { 0x8000000000000000, 1 };

kernel void brace_init_ulong_beyond_long_rejected(device long* out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = kValue.x;
}
