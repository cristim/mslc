// EXPECT: error needs an initialiser
kernel void local_reference_needs_initialiser_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0;
  float &r;
  o[0] = a;
}
