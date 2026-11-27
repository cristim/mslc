// EXPECT: error cannot be narrowed to type
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]], constant uint* un [[buffer(2)]]) { uint u = un[0]; char x{true ? -1 : u}; (void)x; out[0] = 1.0f; }
