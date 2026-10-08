// EXPECT: error a field designator requires a struct initializer
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float f { .a = 1 }; out[0] = f; }
