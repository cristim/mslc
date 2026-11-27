// EXPECT: error narrowed
struct I { int i; };
kernel void k(device float* out [[buffer(0)]]) { I v{.i=1.5f,.i=1}; out[0]=float(v.i); }
