// EXPECT: error is not const but is bound to constant memory
kernel void local_reference_non_const_to_constant_rejected(device float *o [[buffer(0)]], constant float *c [[buffer(1)]]) {
  device float &r = c[0];
  o[0] = r;
}
