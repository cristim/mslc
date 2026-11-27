// EXPECT: error holds is_function_constant_defined(...) and has to be a bool
//
// is_function_constant_defined gives a bool, and the constant that holds it is one.
constant int x [[function_constant(0)]];
constant int has = is_function_constant_defined(x);
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
