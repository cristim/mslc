// EXPECT: error recursive value struct
struct A { B b; }; struct B { A a; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { A s {}; out[0] = 0; }
