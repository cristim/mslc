// EXPECT: error constexpr variable "n" must be initialized by a constant expression
kernel void local_constexpr_divide_by_zero_rejected(device float *out [[buffer(0)]]) {
  constexpr int n = 1 / 0;
  out[0] = float(n);
}
