// EXPECT: error is_function_constant_defined takes the name of a function constant
//
// is_function_constant_defined of a constant that is not a function constant.
constant int z = 3;
kernel void k(device int* out [[buffer(0)]]) { out[0] = is_function_constant_defined(z); }
