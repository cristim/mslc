// EXPECT: error which has no members, requires explicit braces
struct Emp {}; struct OEm { Emp e; float c; };
kernel void k(device float* out [[buffer(0)]]) { OEm o{1.0f}; out[0]=o.c; }
