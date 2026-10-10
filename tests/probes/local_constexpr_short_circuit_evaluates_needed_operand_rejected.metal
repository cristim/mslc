// EXPECT: error constexpr variable "n" must be initialized by a constant expression
// true && x evaluates x, so its division by zero is reached.
kernel void local_constexpr_short_circuit_evaluates_needed_operand_rejected(device float *out [[buffer(0)]]) {
  constexpr bool n = true && (1 / 0 > 0);
  out[0] = float(n);
}
