// EXPECT: error cannot be combined
//
// Apple: 'bool' cannot be signed or unsigned.
kernel void k(device uint* o [[buffer(0)]]) { unsigned bool v = false; o[0] = (uint)v; }
