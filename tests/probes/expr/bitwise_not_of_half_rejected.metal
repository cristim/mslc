// EXPECT: error operator ~ needs an integer operand
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { half h = 0.5h; half r = ~h; out[0] = (float)r; out[0] = 1.0f; }
