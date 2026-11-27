// EXPECT: valid
// DISASM-MATCH: OpCompositeConstruct
// Vector and struct declarators with their own initialisers, one referencing the other.
struct P { float x; float y; };
kernel void several_declarators_vector_and_struct_initialisers(device float *o [[buffer(0)]]) {
  float4 u = float4(1.0), v = u * 2.0, w;
  P p = {1.0, 2.0}, q = p, r;
  w = v;
  r = q;
  o[0] = w.x + r.y;
}
