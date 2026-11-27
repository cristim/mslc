// EXPECT: error which cannot be narrowed to type 'half'
//
// 100000.0f is past the largest half, so converting it changes the value to
// infinity.
constant half2 kValue = { 100000.0f, 1 };

kernel void brace_init_float_beyond_half_rejected(device half* out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = kValue.x;
}
