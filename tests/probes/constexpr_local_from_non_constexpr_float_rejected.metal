// EXPECT: error constexpr variable "c" must be initialized by a constant expression
// A const float is not a constant expression in C++; only a constexpr one is.
kernel void constexpr_local_from_non_constexpr_float_rejected(device float *out [[buffer(0)]]) {
  const float f = 2.0;
  constexpr float c = f;
  out[0] = c;
}
