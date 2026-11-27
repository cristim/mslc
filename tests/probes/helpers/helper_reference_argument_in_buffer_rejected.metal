// EXPECT: error is not a variable of this function
void setf(thread float &x) { x = 1.0; }
kernel void helper_reference_argument_in_buffer_rejected(device float *o [[buffer(0)]]) {
  setf(o[0]);
}
