// EXPECT: valid
// The right operand of && and || is not evaluated once the left decides, so the
// division by zero in it is not part of the constant expression.
kernel void local_constexpr_short_circuit_skips_unevaluated_operand(device float *out [[buffer(0)]]) {
  constexpr bool a = true || (1 / 0 > 0);
  constexpr bool b = false && (1.0f / 0.0f > 0.0f);
  out[0] = float(a) + float(b);
}
