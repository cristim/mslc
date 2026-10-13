// EXPECT: error a multi-dimensional array is not lowered yet
kernel void local_array_two_dimensional_rejected(device float *o [[buffer(0)]]) {
  float a[2][3];
  a[1][2] = 4.0;
  o[0] = a[1][2];
}
