// EXPECT: error cannot be combined
//
// Apple: 'short char' is invalid.
kernel void k(device uint* o [[buffer(0)]]) { short char v = 0; o[0] = (uint)v; }
