// EXPECT: valid
// Only a constexpr needs a constant expression; a const local may overflow (Apple warns).
kernel void local_const_overflow_accepted(device float *out [[buffer(0)]]) {
  const int n = 2147483647 + 1;
  out[0] = float(n);
}
