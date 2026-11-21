// EXPECT: error another argument reads or writes the same storage
// f(a[0], a[1], a): the element references are copies written back after the call, which would overwrite
// the callee's writes through p (Apple: 11 and a = {5, 6}).
float f(thread float &x, thread float &y, thread float *p) { p[0] = 5.0f; p[1] = 6.0f; return x + y; }
kernel void helper_reference_element_and_pointer_to_same_array_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0f, 2.0f};
  o[0] = f(a[0], a[1], a);
}
