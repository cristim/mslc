// EXPECT: error cannot be combined
//
// Apple: cannot combine with previous 'char' declaration specifier.
kernel void k(device uint* o [[buffer(0)]]) { char int v = 0; o[0] = (uint)v; }
