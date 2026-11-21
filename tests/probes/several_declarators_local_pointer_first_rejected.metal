// EXPECT: error a local of type float* is not lowered yet
// The pointer is the first declarator; the float after it is not what the diagnostic names.
kernel void several_declarators_local_pointer_first_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0;
  thread float *p = &a, q = 2.0;
  o[0] = q;
}
