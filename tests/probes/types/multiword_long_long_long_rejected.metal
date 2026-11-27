// EXPECT: error long long
//
// Apple rejects a third long.
kernel void k(device uint* o [[buffer(0)]]) { long long long v = 0; o[0] = (uint)v; }
