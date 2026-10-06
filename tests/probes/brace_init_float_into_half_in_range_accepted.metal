// EXPECT: valid
// DISASM: = OpConstant %half 0x1.8p+0

//
// A float that is in range of a half is not narrowing, even when it rounds, so
// 0.1f is accepted too.
constant half2 kValue = { 1.5f, 1 };

kernel void brace_init_float_into_half_in_range_accepted(device half* out [[buffer(0)]],
                                                         uint i [[thread_position_in_grid]])
{
    out[i] = kValue.x;
}
