// EXPECT: error has the type int2, and mslc lowers scalar function constants only
//
// Metal allows vector function constants; mslc lowers scalars only.
constant int2 x [[function_constant(0)]];
kernel void k(device int* out [[buffer(0)]]) { out[0] = 1; }
