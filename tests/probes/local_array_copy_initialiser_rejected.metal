// EXPECT: error array initializer must be an initializer list
kernel void local_array_copy_initialiser_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0};
  float b[2] = a;
  o[0] = b[1];
}
