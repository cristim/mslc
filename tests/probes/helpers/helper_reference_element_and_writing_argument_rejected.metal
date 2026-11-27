// EXPECT: error another argument reads or writes the same storage
// The third argument writes a[0]; Apple sees the write through x (5), a stale copy would not.
float g(thread float &x, thread float &y, float z) { return x; }
kernel void helper_reference_element_and_writing_argument_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0f, 2.0f};
  o[0] = g(a[0], a[1], (a[0] = 5.0f, 1.0f));
}
