// EXPECT: error the constant "y" is initialised from the function constant "x", and the only such initialiser mslc lowers is is_function_constant_defined(x)
//
// A constant computed from a function constant would be a specialization constant operation, which mslc does not emit.
constant int x [[function_constant(0)]];
constant int y = x * 2;
kernel void k(device int* out [[buffer(0)]]) { out[0] = y; }
