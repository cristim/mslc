// EXPECT: error is a component of a vector
void f(thread float &x) { x = 1.0f; }
kernel void helper_reference_matrix_element_rejected(device float *o [[buffer(0)]]) {
  float2x2 m = float2x2(0.0f);
  f(m[1][0]);
  o[0] = m[1][0];
}
