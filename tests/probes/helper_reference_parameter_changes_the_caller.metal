// EXPECT: valid
// A non-const reference parameter is the caller's variable: the helper's store is
// seen by the caller, so the helper takes a pointer to it rather than a copy.
void bump(thread float &x) { x += 1.0; }
kernel void helper_reference_parameter_changes_the_caller(device float *o [[buffer(0)]]) {
  float a = 1.0;
  bump(a);
  bump(a);
  o[0] = a;
}
