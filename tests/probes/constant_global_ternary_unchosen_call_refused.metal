// EXPECT: error not known outside a function
int f() { return 3; }
constant int g = true ? 1 : f();
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { out[0] = 1.0f; }
