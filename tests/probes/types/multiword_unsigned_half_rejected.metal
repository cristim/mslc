// EXPECT: error cannot be combined
//
// Apple: 'half' cannot be signed or unsigned.
kernel void k(device uint* o [[buffer(0)]]) { unsigned half v = 0; o[0] = (uint)v; }
