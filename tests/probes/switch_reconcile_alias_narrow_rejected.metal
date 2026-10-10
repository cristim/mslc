// EXPECT: error a narrowing initializer list conversion requires a supported constant expression
kernel void k(device float* out [[buffer(0)]]) {
  const int n = 1 + 1;
  float x{n};
  out[0] = x;
}
