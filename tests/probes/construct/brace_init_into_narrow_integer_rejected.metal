// EXPECT: error constant expression evaluates to 256 which cannot be narrowed to type 'unsigned char'

//
// 256 does not fit a uchar.
constant uchar2 kValue = { 256, 1 };

kernel void brace_init_into_narrow_integer_rejected(device uint* out [[buffer(0)]],
                                                    uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
