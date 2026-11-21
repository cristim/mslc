// EXPECT: error may overlap
// The same element twice is one variable through two references.
void f(thread float &x, thread float &y) { x = 1.0f; y = 2.0f; }
kernel void helper_reference_same_literal_index_rejected(device float *o [[buffer(0)]]) {
  float a[3] = {0.0f, 0.0f, 0.0f};
  f(a[1], a[1]);
  o[0] = a[1];
}
