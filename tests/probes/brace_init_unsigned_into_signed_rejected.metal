// EXPECT: error constant expression evaluates to 4294967295 which cannot be narrowed to type 'int'

//
// 4294967295u does not fit an int.
constant int2 kValue = { 4294967295u, 1 };

kernel void brace_init_unsigned_into_signed_rejected(device uint* out [[buffer(0)]],
                                                     uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
