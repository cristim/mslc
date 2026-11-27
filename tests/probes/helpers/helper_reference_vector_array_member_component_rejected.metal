// EXPECT: error is a component of a vector
struct S { float4 av[2]; };
void f(thread float &x) { x = 1.0f; }
kernel void helper_reference_vector_array_member_component_rejected(device float *o [[buffer(0)]]) {
  S s; s.av[0] = float4(0.0f); s.av[1] = float4(0.0f);
  f(s.av[0][1]);
  o[0] = s.av[0].y;
}
