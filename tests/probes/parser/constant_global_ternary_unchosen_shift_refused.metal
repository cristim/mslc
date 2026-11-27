// EXPECT: error not known outside a function
constant int g = true ? 1 : 1 << 40;
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { out[0] = 1.0f; }
