// EXPECT: error constexpr variable "n" must be initialized by a constant expression
kernel void local_constexpr_float_divide_zero_rejected(device float *out [[buffer(0)]]) {
  constexpr float n = 1.0f / 0.0f;
  out[0] = n;
}
