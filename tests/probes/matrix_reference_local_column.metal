// EXPECT: valid
// DISASM-NOT: OpLoad %v4float %[A-Za-z0-9_]+$
// A column indexed through a reference local bound to a buffer place is loaded with the
// Aligned operand a buffer access needs, for a reference parameter and for a struct member.
struct S { float4x4 m; };
kernel void matrix_reference_local_column(device float *o [[buffer(0)]], constant float4x4 &m [[buffer(1)]], constant S &s [[buffer(2)]]) {
  const constant float4x4 &r = m;
  const constant float4x4 &q = s.m;
  float4 c = r[3];
  o[0] = c.w;
  o[1] = q[1].x;
}
