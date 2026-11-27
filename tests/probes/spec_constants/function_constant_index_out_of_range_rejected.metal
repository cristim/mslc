// EXPECT: error needs an index from 0 to 65535
//
// The index of a function constant is at most 65535.
constant int x [[function_constant(65536)]];
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
