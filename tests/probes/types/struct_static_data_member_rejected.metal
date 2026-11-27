// EXPECT: error a static data member is not supported
struct S { float x; static constexpr constant float k = 2; };
kernel void k(device float *out [[buffer(0)]]) { out[0] = S::k; }
