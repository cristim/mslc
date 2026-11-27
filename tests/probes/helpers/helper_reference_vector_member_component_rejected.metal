// EXPECT: error is a component of a vector
struct S { float4 v; };
void f(thread float &x) { x = 1.0f; }
kernel void helper_reference_vector_member_component_rejected(device float *o [[buffer(0)]]) {
  S s; s.v = float4(0.0f);
  f(s.v[1]);
  o[0] = s.v.y;
}
