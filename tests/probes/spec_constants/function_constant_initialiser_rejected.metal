// EXPECT: error has an initialiser, but its value is given when a function is made
//
// A function constant has no initialiser: its value comes from the host.
constant int x [[function_constant(0)]] = 1;
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
