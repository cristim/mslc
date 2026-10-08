// EXPECT: error a constexpr brace initializer requires a supported constant expression
struct Inner { float x; }; struct Outer { Inner a; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { constexpr Outer n = {{in[0]}}; out[0] = n.a.x; }
