// EXPECT: error not known outside a function
constant uint g = true ? 1u : 5u % 0u;
kernel void k(device float* out [[buffer(0)]], constant int* in [[buffer(1)]]) { out[0] = 1.0f; }
