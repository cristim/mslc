// EXPECT: valid
struct S { float a; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { for (S s { in[0] }; s.a > 0; s.a -= 1) { out[0] = s.a; }
}
