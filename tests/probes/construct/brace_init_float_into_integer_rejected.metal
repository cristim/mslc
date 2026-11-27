// EXPECT: error type 'float' cannot be narrowed to 'int' in an initializer list

//
// A float is never narrowed to an integer in braces, whatever its value; Apple
// rejects {1.0f} as well.
constant int2 kValue = { 3.9f, 1 };

kernel void brace_init_float_into_integer_rejected(device uint* out [[buffer(0)]],
                                                   uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
