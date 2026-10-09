// EXPECT: error is const or in constant memory
void setf(thread float &x) { x = 1.0; }
kernel void helper_reference_argument_const_rejected(device float *o [[buffer(0)]]) {
  const float a = 0.0;
  setf(a);
  o[0] = a;
}
