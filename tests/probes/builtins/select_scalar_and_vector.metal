// EXPECT: valid
// DISASM-MATCH: OpSelect %v4float %[a-z0-9_]+ %[a-z0-9_]+ %[a-z0-9_]+
// DISASM-MATCH: OpSelect %int
// DISASM-MATCH: OpFOrdNotEqual %bool|OpFUnordNotEqual %bool
kernel void select_scalar_and_vector(device float4 *o [[buffer(0)]], device int *n [[buffer(1)]],
                                     device const float4 *in [[buffer(2)]]) {
  float4 a = in[0];
  float4 b = in[1];
  o[0] = select(a, b, a > 0.0f);
  o[1] = select(a, 9.0f, bool4(true, false, true, false));
  o[2] = select(a, b, in[2].x);
  n[0] = select(1, 2, a.x > 0.0f);
  n[1] = select(int2(7), int2(8), bool2(false, true)).y;
}
