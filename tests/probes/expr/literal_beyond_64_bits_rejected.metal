// EXPECT: error integer literal is too large to be represented in any integer type
//
// One past the largest ulong. Apple compiles it to 0 without a word, so it is
// rejected here rather than followed.
kernel void literal_beyond_64_bits_rejected(device ulong* out [[buffer(0)]],
                                            uint i [[thread_position_in_grid]])
{
    out[i] = 18446744073709551616;
}
