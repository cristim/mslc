// EXPECT: error constexpr variable "n" must be initialized by a constant expression
kernel void local_constexpr_signed_overflow_rejected(device float *out [[buffer(0)]]) {
  constexpr int n = 2147483647 + 1;
  out[0] = float(n);
}
