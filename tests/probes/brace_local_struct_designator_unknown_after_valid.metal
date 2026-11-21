// EXPECT: error field z is unknown in S initializer
struct S { float a; float b; }; struct T { float a; float b; float c; }; struct P { float a; float b; }; struct O { P p; float c; };
kernel void k(device float* out [[buffer(0)]]) { S s{.a=1.0f,.z=2.0f}; out[0]=s.a; }
