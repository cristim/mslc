// EXPECT: error operator ~ needs an integer operand
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { float f = out[0]; float r = ~f; out[0] = r; out[0] = 1.0f; }
