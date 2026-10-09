// EXPECT: error has to be a variable, an element or a member
void setf(thread float &x) { x = 1.0; }
kernel void helper_reference_argument_value_rejected(device float *o [[buffer(0)]]) {
  setf(2.0);
}
