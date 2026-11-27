// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[A-Za-z0-9_]+ RoundEven
kernel void math_rint_is_round_even(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  o[0] = rint(in[0]);
}
