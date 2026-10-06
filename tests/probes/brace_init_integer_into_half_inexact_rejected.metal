// EXPECT: error constant expression evaluates to 2049 which cannot be narrowed to type 'half'

//
// 2049 is not a half; the next one is 2050.
constant half2 kValue = { 2049, 1 };

kernel void brace_init_integer_into_half_inexact_rejected(device uint* out [[buffer(0)]],
                                                          uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
