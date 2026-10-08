// EXPECT: valid
struct Inner { float x; }; struct Outer { Inner a; };
kernel void k(device float* out [[buffer(0)]]) { constexpr Outer n = {}; out[0] = n.a.x; }
