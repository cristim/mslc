// EXPECT: error operator ~ needs an integer operand
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { half2 h = half2(0.5h); half2 r = ~h; out[0] = (float)r.x; out[0] = 1.0f; }
