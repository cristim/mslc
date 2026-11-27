// EXPECT: error cannot be combined
//
// Apple: 'long char' is invalid.
kernel void k(device uint* o [[buffer(0)]]) { long char v = 0; o[0] = (uint)v; }
