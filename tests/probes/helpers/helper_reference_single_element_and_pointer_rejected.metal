// EXPECT: error another argument reads or writes the same storage
float g(thread float &x, thread float *p) { p[0] = 5.0f; return x; }
kernel void helper_reference_single_element_and_pointer_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0f, 2.0f};
  o[0] = g(a[0], a);
}
