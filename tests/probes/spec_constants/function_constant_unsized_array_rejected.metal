// EXPECT: error the function constant "c" is an array, and mslc lowers scalar function constants only
//
// An array, its length given or taken from an initialiser, is no scalar.
constant int c[] [[function_constant(0)]];
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
