// EXPECT: valid
// DISASM-MATCH: OpCompositeExtract
struct P { float a; float4 v; };
P make(float x) { P p; p.a = x * 2.0; p.v = float4(x, x + 1.0, x + 2.0, x + 3.0); return p; }
kernel void helper_result_member_vector_field(device float *out [[buffer(0)]], constant float *in [[buffer(1)]]) {
  out[0] = make(in[0]).a;
  out[1] = make(in[0]).v.z;
  out[2] = make(in[0]).v.y + make(1.0).a;
}
