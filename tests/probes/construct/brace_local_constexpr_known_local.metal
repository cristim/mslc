// EXPECT: valid
struct Inner { int x; }; struct Outer { Inner a; };
kernel void k(device int* out [[buffer(0)]]) { const int value = 3; constexpr Outer n {{value}}; out[0] = n.a.x; }
