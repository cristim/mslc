// EXPECT: error the function constants "x" and "y" have the same index 0
//
// Two function constants cannot share an index.
constant int x [[function_constant(0)]];
constant int y [[function_constant(0)]];
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
