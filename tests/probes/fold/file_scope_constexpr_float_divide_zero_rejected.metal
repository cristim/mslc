// EXPECT: error constexpr variable "n" must be initialized by a constant expression
constant constexpr float n = 1.0f / 0.0f;
kernel void file_scope_constexpr_float_divide_zero_rejected(device float *out [[buffer(0)]]) {
  out[0] = n;
}
