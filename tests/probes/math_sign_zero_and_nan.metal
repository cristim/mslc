// EXPECT: valid
// DISASM-MATCH: OpIsNan
// DISASM-MATCH: OpFOrdEqual
// Metal: sign(-0.0) is -0.0 and sign(NaN) is 0.0, which GLSL.std.450 FSign leaves open, so the
// zero is selected back and a NaN selected to zero (lavapipe readback of 1.0 / sign(x): -inf for -0.0).
kernel void math_sign_zero_and_nan(device float *o [[buffer(0)]], constant float4 *in [[buffer(1)]]) {
  float4 s = sign(in[0]);
  o[0] = s.x;
  o[1] = s.w;
}
