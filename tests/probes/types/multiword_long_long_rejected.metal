// EXPECT: error long long
//
// Apple: 'long long' is not supported in Metal.
kernel void k(device uint* o [[buffer(0)]]) { long long v = 0; o[0] = (uint)v; }
