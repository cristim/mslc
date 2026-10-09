// EXPECT: error constexpr variable "m" must be initialized by a constant expression
constant constexpr int n = 2147483647;
constant constexpr int m = n + 1;
kernel void file_scope_constexpr_overflow_through_earlier_constant_rejected(device float *out [[buffer(0)]]) {
  out[0] = float(m);
}
