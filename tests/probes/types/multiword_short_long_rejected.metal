// EXPECT: error cannot be combined
//
// Apple: cannot combine with previous 'short' declaration specifier.
kernel void k(device uint* o [[buffer(0)]]) { short long v = 0; o[0] = v; }
