// EXPECT: error constant expression evaluates to 16777217 which cannot be narrowed to type 'float'

//
// 16777217 is not a float; it would round to 16777216.
constant float2 kValue = { 16777217, 1 };

kernel void brace_init_integer_into_float_inexact_rejected(device uint* out [[buffer(0)]],
                                                           uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
