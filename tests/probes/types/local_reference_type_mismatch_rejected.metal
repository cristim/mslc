// EXPECT: error is not the type of the place it is bound to
kernel void local_reference_type_mismatch_rejected(device float *o [[buffer(0)]]) {
  int a = 1;
  float &r = a;
  o[0] = r;
}
