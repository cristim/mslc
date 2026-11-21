// EXPECT: error may overlap
// a[0] and a[i] overlap when i is 0; mslc does not reason about the value of i.
void f(thread float &x, thread float &y) { x = 1.0f; y = 2.0f; }
kernel void helper_reference_dynamic_index_rejected(device float *o [[buffer(0)]]) {
  float a[3] = {0.0f, 0.0f, 0.0f};
  int i = (int)o[1];
  f(a[0], a[i]);
  o[0] = a[0];
}
