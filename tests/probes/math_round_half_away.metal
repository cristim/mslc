// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[A-Za-z0-9_]+ Trunc
// DISASM-MATCH: OpExtInst %float %[A-Za-z0-9_]+ FSign
// DISASM-NOT: OpExtInst %float %[A-Za-z0-9_]+ Round$
// Metal rounds a half away from zero; GLSL.std.450 Round leaves it to the implementation,
// so round is built from Trunc, FAbs and FSign (lavapipe readback: -2.5 -> -3, 0.49999997 -> 0).
kernel void math_round_half_away(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  o[0] = round(in[0]);
}
