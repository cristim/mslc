// EXPECT: error constant expression evaluates to -1 which cannot be narrowed to type 'unsigned long'

//
// -1 does not fit a ulong, which a wrapping conversion would not notice: it keeps
// all 64 bits.
constant ulong2 kValue = { -1, 1 };

kernel void brace_init_unsigned_into_ulong_negative_rejected(device uint* out [[buffer(0)]],
                                                             uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
