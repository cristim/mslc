// EXPECT: error cannot be combined
//
// Apple: cannot combine with previous 'int' declaration specifier.
kernel void k(device uint* o [[buffer(0)]]) { int int v = 0; o[0] = (uint)v; }
