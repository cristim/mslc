// EXPECT: error attribute "buffer" does not apply to a file-scope constant
//
// A file-scope constant takes no attribute but [[function_constant(n)]].
constant int x [[buffer(0)]];
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
