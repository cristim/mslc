// EXPECT: error constexpr variable "n" must be initialized by a constant expression
constant constexpr int n = -(-2147483647 - 1);
kernel void file_scope_constexpr_most_negative_negated_rejected(device float *out [[buffer(0)]]) {
  out[0] = float(n);
}
