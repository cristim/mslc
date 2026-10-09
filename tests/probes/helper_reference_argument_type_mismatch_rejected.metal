// EXPECT: error is not the type of parameter
void setf(thread float &x) { x = 1.0; }
kernel void helper_reference_argument_type_mismatch_rejected(device float *o [[buffer(0)]]) {
  int a = 0;
  setf(a);
  o[0] = float(a);
}
