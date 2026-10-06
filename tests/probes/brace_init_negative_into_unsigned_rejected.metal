// EXPECT: error constant expression evaluates to -1 which cannot be narrowed to type 'unsigned int'

//
// -1 does not fit a uint, so it cannot be narrowed into one in braces.
constant uint2 kValue = { -1, 1 };

kernel void brace_init_negative_into_unsigned_rejected(device uint* out [[buffer(0)]],
                                                       uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
