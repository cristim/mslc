// EXPECT: error cannot be combined
//
// Apple: cannot combine with previous 'signed' declaration specifier.
kernel void k(device uint* o [[buffer(0)]]) { signed unsigned v = 0; o[0] = v; }
