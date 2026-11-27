// EXPECT: valid
// DISASM-MATCH: OpExtInst %float %[0-9]+ NMin %
// DISASM-MATCH: OpExtInst %float %[0-9]+ NMax %
// DISASM-MATCH: OpExtInst %float %[0-9]+ NClamp %
// Metal returns the other operand of min and max when one is a NaN, and clamp and saturate of a
// NaN give the lower bound; GLSL.std.450 FMin, FMax and FClamp leave that open, so the NaN-aware
// forms are used (Apple GPU: min(2, NaN) = 2, clamp(NaN, 0, 1) = 0, saturate(NaN) = 0).
kernel void math_min_max_clamp_ignore_nan(device float *o [[buffer(0)]], constant float *a [[buffer(1)]]) {
  float x = a[0];
  float n = NAN;
  o[0] = min(x, n);
  o[1] = max(n, x);
  o[2] = clamp(n, 0.0, 1.0);
  o[3] = saturate(n);
}
