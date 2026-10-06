// EXPECT: error cannot be combined
//
// Apple: expected ';' at end of declaration.
kernel void k(device uint* o [[buffer(0)]]) { unsigned uint v = 0; o[0] = v; }
