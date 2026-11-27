// EXPECT: error cannot be combined
//
// Apple: 'double' is not supported in Metal.
kernel void k(device uint* o [[buffer(0)]]) { long double v = 0; o[0] = (uint)v; }
