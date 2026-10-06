// EXPECT: error cannot be combined
//
// Apple: 'float' cannot be signed or unsigned.
kernel void k(device uint* o [[buffer(0)]]) { unsigned float v = 0; o[0] = (uint)v; }
