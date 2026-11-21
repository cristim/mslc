// EXPECT: error a scalar initializer list needs a scalar value
struct Pair { float a; float b; }; struct Outer2 { float x; Pair p; float c; };
kernel void k(device float* out [[buffer(0)]]) { Pair q{1.0f,2.0f}; Outer2 o{q,3.0f}; out[0]=o.c; }
