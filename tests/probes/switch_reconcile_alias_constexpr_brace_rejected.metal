// EXPECT: error a constexpr brace initializer requires a supported constant expression
kernel void k(device int* out [[buffer(0)]]) {
  const int n = 1 + 1;
  constexpr int x{n};
  out[0] = x;
}
