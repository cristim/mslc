// EXPECT: error constant expression evaluates to 2147483648 which cannot be narrowed to type 'int'

//
// A braced initialiser may not change a value. 2147483648 is a long and does not fit
// an int; Apple rejects it as a narrowing conversion, where the plain
// "constant int k = 2147483648;" is only a warning and wraps.
constant int2 kValue = { 2147483648, 1 };

kernel void brace_init_integer_too_large_rejected(device uint* out [[buffer(0)]],
                                                  uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
