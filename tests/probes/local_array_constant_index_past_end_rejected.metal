// EXPECT: error is outside the array
// Apple only warns, and a constant index past the end would be an invalid access chain.
kernel void local_array_constant_index_past_end_rejected(device float *o [[buffer(0)]]) {
  float a[2] = {1.0, 2.0};
  o[0] = a[5];
}
