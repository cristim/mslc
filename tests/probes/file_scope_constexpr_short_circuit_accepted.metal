// EXPECT: valid
constant constexpr bool a = false && (1 / 0 > 0);
constant constexpr bool b = true && false;
kernel void file_scope_constexpr_short_circuit_accepted(device float *out [[buffer(0)]]) {
  out[0] = float(a) + float(b);
}
