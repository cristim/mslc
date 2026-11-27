// EXPECT: error functional cast
//
// Same as unsigned int(x): Apple rejects it.
kernel void k(device uint* o [[buffer(0)]]) { float x = 1; o[0] = long unsigned(x); }
